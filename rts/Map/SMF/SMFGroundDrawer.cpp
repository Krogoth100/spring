/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "SMFReadMap.h"
#include "SMFGroundDrawer.h"
#include "SMFRenderState.h"
#include "Game/Camera.h"
#include "Map/MapInfo.h"
#include "Map/HeightMapTexture.h"
#include "Map/ReadMap.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/ShadowHandler.h"
#include "Rendering/Env/ISky.h"
#include "Rendering/Env/WaterRendering.h"
#include "Rendering/Env/MapRendering.h"
#include "Rendering/GL/myGL.h"
#include "Rendering/GL/RenderBuffers.h"
#include "Rendering/Shaders/Shader.h"
#include "System/Config/ConfigHandler.h"
#include "System/EventHandler.h"
#include "System/FastMath.h"
#include "System/SafeUtil.h"
#include "System/Log/ILog.h"
#include "System/SpringMath.h"

CONFIG(int, MaxDynamicMapLights)
	.defaultValue(1)
	.minimumValue(0).description("Maximum number of map-global dynamic lights that will be rendered at once. High numbers of lights cost performance, as they affect every map fragment.");

CONFIG(bool, AdvMapShading).defaultValue(true).safemodeValue(false).description("Enable shaders for terrain rendering.");
CONFIG(bool, AllowDeferredMapRendering).defaultValue(false).safemodeValue(false).description("Enable rendering the map to the map deferred buffers.");
CONFIG(bool, AllowDrawMapPostDeferredEvents).defaultValue(false).description("Enable DrawGroundPostDeferred Lua callin.");
CONFIG(bool, AllowDrawMapDeferredEvents).defaultValue(false).description("Enable DrawGroundDeferred Lua callin.");


CONFIG(bool, AlwaysSendDrawGroundEvents)
	.defaultValue(false)
	.description("Always send DrawGround{Pre,Post}{Forward,Deferred} events");

namespace Shader {
	struct IProgramObject;
}

CSMFGroundDrawer::CSMFGroundDrawer(CSMFReadMap* rm)
	: smfMap(rm)
	, geomBuffer{"GROUNDDRAWER-GBUFFER"}
{
	alwaysDispatchEvents = configHandler->GetBool("AlwaysSendDrawGroundEvents");

	smfRenderStates = { nullptr };
	smfRenderStates[RENDER_STATE_SSP] = ISMFRenderState::GetInstance(false, false);
	smfRenderStates[RENDER_STATE_LUA] = ISMFRenderState::GetInstance( true, false);
	smfRenderStates[RENDER_STATE_NOP] = ISMFRenderState::GetInstance(false,  true);

	borderShader = shaderHandler->CreateProgramObject("[SMFGroundDrawer]", "Border");
	borderShader->AttachShaderObject(shaderHandler->CreateShaderObject("GLSL/SMFBorderVertProg.glsl", "", GL_VERTEX_SHADER));
	borderShader->AttachShaderObject(shaderHandler->CreateShaderObject("GLSL/SMFBorderFragProg.glsl", "", GL_FRAGMENT_SHADER));
	borderShader->BindAttribLocation("vertexPos", 0);
	borderShader->BindAttribLocation("vertexCol", 1);
	borderShader->Link();

	borderShader->Enable();
	borderShader->SetUniform("diffuseTex"  , 0);
	borderShader->SetUniform("heightMapTex", 1);
	borderShader->SetUniform("detailsTex"  , 2);
	borderShader->SetUniform("mapSize",
		static_cast<float>(mapDims.mapx * SQUARE_SIZE), static_cast<float>(mapDims.mapy * SQUARE_SIZE),
				   1.0f / (mapDims.mapx * SQUARE_SIZE),            1.0f / (mapDims.mapy * SQUARE_SIZE)
	);
	borderShader->Disable();

	borderShader->Validate();

	drawForward = true;
	drawDeferred = geomBuffer.Valid();
	postDeferredEvents = configHandler->GetBool("AllowDrawMapPostDeferredEvents");
	deferredEvents = configHandler->GetBool("AllowDrawMapDeferredEvents");


	if (smfRenderStates[RENDER_STATE_SSP]->Init(this))
		smfRenderStates[RENDER_STATE_SSP]->Update(this, nullptr);

	// always initialize this state; defer Update (allows re-use)
	smfRenderStates[RENDER_STATE_LUA]->Init(this);

	// note: state must be pre-selected before the first drawn frame
	// Sun*Changed can be called first, e.g. if DynamicSun is enabled
	smfRenderStates[RENDER_STATE_SEL] = SelectRenderState(DrawPass::Normal);

	if (drawDeferred) {
		drawDeferred &= UpdateGeometryBuffer(true);
	}
}

CSMFGroundDrawer::~CSMFGroundDrawer()
{
	smfRenderStates[RENDER_STATE_SSP]->Kill(); ISMFRenderState::FreeInstance(smfRenderStates[RENDER_STATE_SSP]);
	smfRenderStates[RENDER_STATE_LUA]->Kill(); ISMFRenderState::FreeInstance(smfRenderStates[RENDER_STATE_LUA]);
	smfRenderStates[RENDER_STATE_NOP]->Kill(); ISMFRenderState::FreeInstance(smfRenderStates[RENDER_STATE_NOP]);

	smfRenderStates = { nullptr };

	shaderHandler->ReleaseProgramObject("[SMFGroundDrawer]", "Border");
}



ISMFRenderState* CSMFGroundDrawer::SelectRenderState(const DrawPass::e& drawPass)
{
	// [0] := Lua GLSL, must have a valid shader for this pass
	// [1] := default ARB *or* GLSL, same condition
	const unsigned int stateEnums[2] = {RENDER_STATE_LUA, RENDER_STATE_SSP};

	for (unsigned int n = 0; n < 2; n++) {
		ISMFRenderState* state = smfRenderStates[ stateEnums[n] ];

		if (!state->HasValidShader(drawPass))
			continue;

		return (smfRenderStates[RENDER_STATE_SEL] = state);
	}

	// fallback
	return (smfRenderStates[RENDER_STATE_SEL] = smfRenderStates[RENDER_STATE_NOP]);
}

bool CSMFGroundDrawer::HaveLuaRenderState() const
{
	return (smfRenderStates[RENDER_STATE_SEL] == smfRenderStates[RENDER_STATE_LUA]);
}



void CSMFGroundDrawer::DrawDeferredPass(const DrawPass::e& drawPass, bool alphaTest)
{
	if (!geomBuffer.Valid())
		return;

	// some water renderers use FBO's for the reflection pass
	if (drawPass == DrawPass::WaterReflection)
		return;
	// some water renderers use FBO's for the refraction pass
	if (drawPass == DrawPass::WaterRefraction)
		return;
	// CubeMapHandler also uses an FBO for this pass
	if (drawPass == DrawPass::TerrainReflection)
		return;

	// deferred pass must be executed with GLSL shaders
	// if the FFP or ARB state was selected, bail early
	if (!SelectRenderState(DrawPass::TerrainDeferred)->CanDrawDeferred(this)) {
		geomBuffer.Bind();
		geomBuffer.SetDepthRange(1.0f, 0.0f);
		geomBuffer.Clear();
		geomBuffer.SetDepthRange(0.0f, 1.0f);
		geomBuffer.UnBind();
		return;
	}

	GL::GeometryBuffer::LoadViewport();

	{
		geomBuffer.Bind();
		geomBuffer.SetDepthRange(1.0f, 0.0f);
		geomBuffer.Clear();

		smfRenderStates[RENDER_STATE_SEL]->SetCurrentShader(this, DrawPass::TerrainDeferred);
		smfRenderStates[RENDER_STATE_SEL]->Enable(this, DrawPass::TerrainDeferred);

		if (alphaTest) {
			glEnable(GL_ALPHA_TEST);
			glAlphaFunc(GL_GREATER, mapInfo->map.voidAlphaMin);
		}

		if (alwaysDispatchEvents || HaveLuaRenderState())
			eventHandler.DrawGroundPreDeferred();

		if (alphaTest) {
			glDisable(GL_ALPHA_TEST);
		}

		smfRenderStates[RENDER_STATE_SEL]->Disable(this, drawPass);
		smfRenderStates[RENDER_STATE_SEL]->SetCurrentShader(this, DrawPass::Normal);

		if (deferredEvents)
			eventHandler.DrawGroundDeferred();

		geomBuffer.SetDepthRange(0.0f, 1.0f);
		geomBuffer.UnBind();
	}

	globalRendering->LoadViewport();

	#if 0
	geomBuffer.DrawDebug(geomBuffer.GetBufferTexture(GL::GeometryBuffer::ATTACHMENT_NORMTEX));
	#endif

	// send event if no forward pass will follow; must be done after the unbind
	if (!drawForward || postDeferredEvents)
		eventHandler.DrawGroundPostDeferred();
}

void CSMFGroundDrawer::DrawForwardPass(const DrawPass::e& drawPass, bool alphaTest)
{
	if (!SelectRenderState(drawPass)->CanDrawForward(this))
		return;

	smfRenderStates[RENDER_STATE_SEL]->SetCurrentShader(this, drawPass);
	smfRenderStates[RENDER_STATE_SEL]->Enable(this, drawPass);

	glPushAttrib((GL_ENABLE_BIT * alphaTest) | (GL_POLYGON_BIT * wireframe));

	if (wireframe)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	if (alphaTest) {
		glEnable(GL_ALPHA_TEST);
		glAlphaFunc(GL_GREATER, mapInfo->map.voidAlphaMin);
	}

	if (alwaysDispatchEvents || HaveLuaRenderState())
		eventHandler.DrawGroundPreForward();

	glPopAttrib();

	smfRenderStates[RENDER_STATE_SEL]->Disable(this, drawPass);
	smfRenderStates[RENDER_STATE_SEL]->SetCurrentShader(this, DrawPass::Normal);

	if (alwaysDispatchEvents || HaveLuaRenderState())
		eventHandler.DrawGroundPostForward();
}

void CSMFGroundDrawer::Draw(const DrawPass::e& drawPass)
{
	// must be here because water renderers also call us
	if (!globalRendering->drawGround)
		return;
	// if entire map is under voidwater, no need to draw *ground*
	if (readMap->HasOnlyVoidWater())
		return;

	glDisable(GL_BLEND);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	if (drawDeferred) {
		// do the deferred pass first, will allow us to re-use
		// its output at some future point and eventually draw
		// the entire map deferred
		DrawDeferredPass(drawPass, mapRendering->voidGround || (mapRendering->voidWater && drawPass != DrawPass::WaterReflection));
	}

	if (drawForward) {
		DrawForwardPass(drawPass, mapRendering->voidGround || (mapRendering->voidWater && drawPass != DrawPass::WaterReflection));
	}

	glDisable(GL_CULL_FACE);
}


// todo: remove
void CSMFGroundDrawer::DrawShadowPass()
{
}



void CSMFGroundDrawer::SetLuaShader(const LuaMapShaderData* luaMapShaderData)
{
	smfRenderStates[RENDER_STATE_LUA]->Update(this, luaMapShaderData);
}

void CSMFGroundDrawer::SetupBigSquare(const DrawPass::e& drawPass, const int bigSquareX, const int bigSquareY)
{
	if (drawPass != DrawPass::Shadow) {
		smfRenderStates[RENDER_STATE_SEL]->SetSquareTexGen(bigSquareX, bigSquareY);

		if (borderShader && borderShader->IsBound()) {
			borderShader->SetUniform("texSquare", bigSquareX, bigSquareY);
		}
	}
	else {
		if (shadowShader && shadowShader->IsBound()) {
			shadowShader->SetUniform("texSquare", bigSquareX, bigSquareY);
		}
	}
}



void CSMFGroundDrawer::Update()
{
	if (readMap->HasOnlyVoidWater())
		return;

	if (drawDeferred) {
		drawDeferred &= UpdateGeometryBuffer(false);
	}
}

void CSMFGroundDrawer::UpdateRenderState()
{
	smfRenderStates[RENDER_STATE_SSP]->Update(this, nullptr);
}

void CSMFGroundDrawer::SunChanged() {
	// Lua has gl.GetSun
	if (HaveLuaRenderState())
		return;

	smfRenderStates[RENDER_STATE_SEL]->UpdateShaderSkyUniforms();
}


bool CSMFGroundDrawer::UpdateGeometryBuffer(bool init)
{
	static const bool drawDeferredAllowed = configHandler->GetBool("AllowDeferredMapRendering");

	if (!drawDeferredAllowed)
		return false;

	return (geomBuffer.Update(init));
}



void CSMFGroundDrawer::SetDetail(int newGroundDetail)
{
}



int CSMFGroundDrawer::GetGroundDetail(const DrawPass::e& drawPass) const
{
	return 0;
}
