/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include <cfloat>

#include "ShadowHandler.h"
#include "Game/Camera.h"
#include "Game/CameraHandler.h"
#include "Game/GameVersion.h"
#include "Map/Ground.h"
#include "Map/MapInfo.h"
#include "Map/ReadMap.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/Features/FeatureDrawer.h"
#include "Rendering/Units/UnitDrawer.h"
#include "Rendering/Env/GrassDrawer.h"
#include "Rendering/Env/ISky.h"
#include "Rendering/GL/FBO.h"
#include "Rendering/GL/myGL.h"
#include "Rendering/Shaders/ShaderHandler.h"
#include "Rendering/Shaders/Shader.h"
#include "Rendering/GL/RenderBuffers.h"
#include "System/Config/ConfigHandler.h"
#include "System/EventHandler.h"
#include "System/Matrix44f.h"
#include "System/SpringMath.h"
#include "System/StringUtil.h"
#include "System/Log/ILog.h"

#include "lib/fmt/format.h"

CONFIG(int, Shadows).defaultValue(2).headlessValue(-1).minimumValue(-1).safemodeValue(-1).description("Sets whether shadows are rendered.\n-1:=forceoff, 0:=off, 1:=full, 2:=fast (skip terrain)"); //FIXME document bitmask
CONFIG(int, ShadowProjectionMode).defaultValue(CShadowHandler::SHADOWPROMODE_CAM_CENTER);
CONFIG(bool, ShadowColorMode).defaultValue(true).description("Whether the colorbuffer of shadowmap FBO is RGB vs greyscale(to conserve some VRAM)");

CShadowHandler shadowHandler;

void CShadowHandler::Init()
{
	const bool tmpFirstInit = firstInit;
	firstInit = false;

	shadowConfig  = configHandler->GetInt("Shadows");
	// disabled; other option usually produces worse resolution
	shadowProMode = configHandler->GetInt("ShadowProjectionMode");
	//shadowProMode = SHADOWPROMODE_CAM_CENTER;
	shadowColorMode = configHandler->GetInt("ShadowColorMode");
	shadowGenBits = SHADOWGEN_BIT_NONE;

	shadowsLoaded = false;
	inShadowPass = false;

	if (!tmpFirstInit && !shadowsSupported)
		return;

	// possible values for the "Shadows" config-parameter:
	// < 0: disable and don't try to initialize
	//   0: disable, but still check if the hardware is able to run them
	// > 0: enabled (by default for all shadow-casting geometry if equal to 1)
	if (shadowConfig < 0) {
		LOG("[%s] shadow rendering is disabled (config-value %d)", __func__, shadowConfig);
		return;
	}

	if (shadowConfig > 0)
		shadowGenBits = SHADOWGEN_BIT_MODEL | SHADOWGEN_BIT_MAP | SHADOWGEN_BIT_PROJ | SHADOWGEN_BIT_TREE;

	if (shadowConfig > 1)
		shadowGenBits &= (~shadowConfig);

	// no warnings when running headless
	if (SpringVersion::IsHeadless())
		return;

	if (tmpFirstInit)
		shadowsSupported = true;

	if (shadowConfig == 0) {
		// shadowsLoaded is still false
		return;
	}

	LoadProjectionMatrix(CCameraHandler::GetCamera(CCamera::CAMTYPE_SHADOW));
}


void CShadowHandler::Update()
{
	CCamera* playCam = CCameraHandler::GetCamera(CCamera::CAMTYPE_PLAYER);
	CCamera* shadCam = CCameraHandler::GetCamera(CCamera::CAMTYPE_SHADOW);

	SetShadowMatrix(playCam, shadCam);
	SetShadowCamera(shadCam);
}

void CShadowHandler::DrawFrustumDebug() const
{
	if (!debugFrustum || !shadowsLoaded)
		return;

	CCamera* shadCam = CCameraHandler::GetCamera(CCamera::CAMTYPE_SHADOW);

	auto& rb = RenderBuffer::GetTypedRenderBuffer<VA_TYPE_0>();
	rb.AssertSubmission();

	rb.AddVertices({ { shadCam->GetFrustumVert(0) }, { shadCam->GetFrustumVert(1) } }); // NBL - NBR
	rb.AddVertices({ { shadCam->GetFrustumVert(1) }, { shadCam->GetFrustumVert(2) } }); // NBR - NTR
	rb.AddVertices({ { shadCam->GetFrustumVert(2) }, { shadCam->GetFrustumVert(3) } }); // NTR - NTL
	rb.AddVertices({ { shadCam->GetFrustumVert(3) }, { shadCam->GetFrustumVert(0) } }); // NTL - NBL

	rb.AddVertices({ { shadCam->GetFrustumVert(3) }, { shadCam->GetFrustumVert(7) } }); // NTL - FTL
	rb.AddVertices({ { shadCam->GetFrustumVert(2) }, { shadCam->GetFrustumVert(6) } }); // NTR - FTR
	rb.AddVertices({ { shadCam->GetFrustumVert(0) }, { shadCam->GetFrustumVert(4) } }); // NBL - FBL
	rb.AddVertices({ { shadCam->GetFrustumVert(1) }, { shadCam->GetFrustumVert(5) } }); // NBR - FBR

	rb.AddVertices({ { shadCam->GetFrustumVert(4) }, { shadCam->GetFrustumVert(5) } }); // FBL - FBR
	rb.AddVertices({ { shadCam->GetFrustumVert(5) }, { shadCam->GetFrustumVert(6) } }); // FBR - FTR
	rb.AddVertices({ { shadCam->GetFrustumVert(6) }, { shadCam->GetFrustumVert(7) } }); // FTR - FTL
	rb.AddVertices({ { shadCam->GetFrustumVert(7) }, { shadCam->GetFrustumVert(4) } }); // FTL - FBL

	auto& sh = rb.GetShader();
	glLineWidth(2.0f);
	sh.Enable();
	sh.SetUniform("ucolor", 0.0f, 0.0f, 1.0f, 1.0f);
	rb.DrawArrays(GL_LINES);
	sh.SetUniform("ucolor", 1.0f, 1.0f, 1.0f, 1.0f);
	sh.Disable();
	glLineWidth(1.0f);
}


void CShadowHandler::LoadProjectionMatrix(const CCamera* shadowCam)
{
	const CMatrix44f& ccm = shadowCam->GetClipControlMatrix();
	      CMatrix44f& spm = projMatrix[SHADOWMAT_TYPE_DRAWING];

	// same as glOrtho(-1, 1,  -1, 1,  -1, 1); just inverts Z
	// spm.LoadIdentity();
	// spm.SetZ(-FwdVector);

	// same as glOrtho(0, 1,  0, 1,  0, -1); maps [0,1] to [-1,1]
	spm.LoadIdentity();
	spm.Translate(-OnesVector);
	spm.Scale(OnesVector * 2.0f);

	// if using ZTO clip-space, cancel out the above remap for Z
	spm = ccm * spm;
}


static CMatrix44f ComposeLightMatrix(const CCamera* playerCam, const ISkyLight* light)
{
	CMatrix44f lightMatrix;

	// sun direction is in world-space, invert it
	float3 zDir = -float3(light->GetLightDir());

	// Try to rotate LM's X and Y around Z direction to fit playerCam tightest

	// find the most orthogonal vector to zDir and call it xDir
	float minDot = 1.0f;
	float3 xDir;
	for (const auto* dir : { &playerCam->forward, &playerCam->right, &playerCam->up }) {
		const float dp = zDir.dot(*dir);
		if (math::fabs(dp) < minDot) {
			xDir = std::copysign(1.0f, dp) * (*dir);
			minDot = math::fabs(dp);
		}
	}

	// orthonormalize
	xDir = (xDir - xDir.dot(zDir) * zDir).ANormalize();
	float3 yDir = xDir.cross(zDir).ANormalize();

	lightMatrix.SetZ(zDir);
	lightMatrix.SetY(yDir);
	lightMatrix.SetX(xDir);

	return lightMatrix;
}

static CMatrix44f ComposeScaleMatrix(const float4 scales)
{
	// note: T is z-bias, scales.z is z-near
	return (CMatrix44f(FwdVector * 0.5f, RgtVector / scales.x, UpVector / scales.y, FwdVector / scales.w));
}

void CShadowHandler::SetShadowMatrix(CCamera* playerCam, CCamera* shadowCam)
{
	const CMatrix44f lightMatrix = ComposeLightMatrix(playerCam, ISky::GetSky()->GetLight());
	const CMatrix44f scaleMatrix = ComposeScaleMatrix(shadowProjScales = GetShadowProjectionScales(playerCam, lightMatrix));

	// KISS; define only the world-to-light transform (P[CULLING] is unused anyway)
	//
	// we have two options: either place the camera such that it *looks at* projMidPos
	// (along lightMatrix.GetZ()) or such that it is *at or behind* projMidPos looking
	// in the inverse direction (the latter is chosen here since this matrix determines
	// the shadow-camera's position and thereby terrain tessellation shadow-LOD)
	// NOTE:
	//   should be -X-Z, but particle-quads are sensitive to right being flipped
	//   we can omit inverting X (does not impact VC) or disable PD face-culling
	//   or just let objects end up behind znear since InView only tests against
	//   zfar
	viewMatrix[SHADOWMAT_TYPE_CULLING].LoadIdentity();
	viewMatrix[SHADOWMAT_TYPE_CULLING].SetX(lightMatrix.GetX());
	viewMatrix[SHADOWMAT_TYPE_CULLING].SetY(lightMatrix.GetY());
	viewMatrix[SHADOWMAT_TYPE_CULLING].SetZ(lightMatrix.GetZ());
	viewMatrix[SHADOWMAT_TYPE_CULLING].SetPos(projMidPos[2]);

	// shaders need this form, projection into SM-space is done by shadow2DProj()
	// note: ShadowGenVertProg is a special case because it does not use uniforms
	viewMatrix[SHADOWMAT_TYPE_DRAWING].LoadIdentity();
	viewMatrix[SHADOWMAT_TYPE_DRAWING].SetX(lightMatrix.GetX());
	viewMatrix[SHADOWMAT_TYPE_DRAWING].SetY(lightMatrix.GetY());
	viewMatrix[SHADOWMAT_TYPE_DRAWING].SetZ(lightMatrix.GetZ());
	viewMatrix[SHADOWMAT_TYPE_DRAWING].Scale(float3(scaleMatrix[0], scaleMatrix[5], scaleMatrix[10])); // extract (X.x, Y.y, Z.z)
	viewMatrix[SHADOWMAT_TYPE_DRAWING].Transpose();
	viewMatrix[SHADOWMAT_TYPE_DRAWING].SetPos(viewMatrix[SHADOWMAT_TYPE_DRAWING] * -projMidPos[2]);
	viewMatrix[SHADOWMAT_TYPE_DRAWING].SetPos(viewMatrix[SHADOWMAT_TYPE_DRAWING].GetPos() + scaleMatrix.GetPos()); // add z-bias
}

void CShadowHandler::SetShadowCamera(CCamera* shadowCam)
{
	// first set matrices needed by shaders (including ShadowGenVertProg)
	shadowCam->SetProjMatrix(projMatrix[SHADOWMAT_TYPE_DRAWING]);
	shadowCam->SetViewMatrix(viewMatrix[SHADOWMAT_TYPE_DRAWING]);

	shadowCam->SetAspectRatio(shadowProjScales.x / shadowProjScales.y);
	// convert xy-diameter to radius
	shadowCam->SetFrustumScales(shadowProjScales * float4(0.5f, 0.5f, 1.0f, 1.0f));
	shadowCam->UpdateFrustum();
	// todo: -shadow render cleanup
	//shadowCam->UpdateLoadViewport(0, 0, shadowMapSize, shadowMapSize);
	shadowCam->UpdateLoadViewport(0, 0, 1, 1);
	// load matrices into gl_{ModelView,Projection}Matrix
	shadowCam->Update({false, false, false, false, false});

	// next set matrices needed for SP visibility culling (these
	// are *NEVER* loaded into gl_{ModelView,Projection}Matrix!)
	shadowCam->SetProjMatrix(projMatrix[SHADOWMAT_TYPE_CULLING]);
	shadowCam->SetViewMatrix(viewMatrix[SHADOWMAT_TYPE_CULLING]);
	shadowCam->UpdateFrustum();
}


float4 CShadowHandler::GetShadowProjectionScales(CCamera* playerCam, const CMatrix44f& lightViewMat) {
	float4 projScales;
	float2 projRadius;

	// NOTE:
	//   the xy-scaling factors from CalcMinMaxView do not change linearly
	//   or smoothly with camera movements, creating visible artefacts (eg.
	//   large jumps in shadow resolution)
	//
	//   therefore, EITHER use "fixed" scaling values such that the entire
	//   map barely fits into the sun's frustum (by pretending it is embedded
	//   in a sphere and taking its diameter), OR variable scaling such that
	//   everything that can be seen by the camera maximally fills the sun's
	//   frustum (choice of projection-style is left to the user and can be
	//   changed at run-time)
	//
	//   the first option means larger maps will have more blurred/aliased
	//   shadows if the depth buffer is kept at the same size, but no (map)
	//   geometry is ever omitted
	//
	//   the second option means shadows have higher average resolution, but
	//   become less sharp as the viewing volume increases (through eg.camera
	//   rotations) and geometry can be omitted in some cases
	//
	switch (shadowProMode) {
		case SHADOWPROMODE_CAM_CENTER: {
			projScales.x = GetOrthoProjectedFrustumRadius(playerCam, lightViewMat, projMidPos[2]);
		} break;
		case SHADOWPROMODE_MAP_CENTER: {
			projScales.x = GetOrthoProjectedMapRadius(-lightViewMat.GetZ(), projMidPos[2]);
		} break;
		case SHADOWPROMODE_MIX_CAMMAP: {
			projRadius.x = GetOrthoProjectedFrustumRadius(playerCam, lightViewMat, projMidPos[0]);
			projRadius.y = GetOrthoProjectedMapRadius(-lightViewMat.GetZ(), projMidPos[1]);
			projScales.x = std::min(projRadius.x, projRadius.y);

			// pick the center position (0 or 1) for which radius is smallest
			projMidPos[2] = projMidPos[projRadius.x >= projRadius.y];
		} break;
	}

	projScales.y = projScales.x;
	#if 0
	projScales.z = cam->GetNearPlaneDist();
	projScales.w = cam->GetFarPlaneDist();
	#else
	// prefer slightly tighter fixed bounds
	projScales.z = 0.0f;
	projScales.w = readMap->GetBoundingRadius() * 2.0f;
	#endif
	return projScales;
}

float CShadowHandler::GetOrthoProjectedMapRadius(const float3& sunDir, float3& projPos) {
	// to fit the map inside the frustum, we need to know
	// the distance from one corner to its opposing corner
	//
	// this distance is maximal when the sun direction is
	// orthogonal to the diagonal, but in other cases we
	// can gain some precision by projecting the diagonal
	// onto a vector orthogonal to the sun direction and
	// using the length of that projected vector instead
	//
	const float maxMapDiameter = readMap->GetBoundingRadius() * 2.0f;
	static float curMapDiameter = 0.0f;

	// recalculate pos only if the sun-direction has changed
	if (sunProjDir != sunDir) {
		sunProjDir = sunDir;

		float3 sunDirXZ = (sunDir * XZVector).ANormalize();
		float3 mapVerts[2];

		if (sunDirXZ.x >= 0.0f) {
			if (sunDirXZ.z >= 0.0f) {
				// use diagonal vector from top-right to bottom-left
				mapVerts[0] = float3(mapDims.mapx * SQUARE_SIZE, 0.0f,                       0.0f);
				mapVerts[1] = float3(                      0.0f, 0.0f, mapDims.mapy * SQUARE_SIZE);
			} else {
				// use diagonal vector from top-left to bottom-right
				mapVerts[0] = float3(                      0.0f, 0.0f,                       0.0f);
				mapVerts[1] = float3(mapDims.mapx * SQUARE_SIZE, 0.0f, mapDims.mapy * SQUARE_SIZE);
			}
		} else {
			if (sunDirXZ.z >= 0.0f) {
				// use diagonal vector from bottom-right to top-left
				mapVerts[0] = float3(mapDims.mapx * SQUARE_SIZE, 0.0f, mapDims.mapy * SQUARE_SIZE);
				mapVerts[1] = float3(                      0.0f, 0.0f,                       0.0f);
			} else {
				// use diagonal vector from bottom-left to top-right
				mapVerts[0] = float3(                      0.0f, 0.0f, mapDims.mapy * SQUARE_SIZE);
				mapVerts[1] = float3(mapDims.mapx * SQUARE_SIZE, 0.0f,                       0.0f);
			}
		}

		const float3 v1 = (mapVerts[1] - mapVerts[0]).ANormalize();
		const float3 v2 = float3(-sunDirXZ.z, 0.0f, sunDirXZ.x);

		curMapDiameter = maxMapDiameter * v2.dot(v1);

		projPos.x = (mapDims.mapx * SQUARE_SIZE) * 0.5f;
		projPos.z = (mapDims.mapy * SQUARE_SIZE) * 0.5f;
		projPos.y = CGround::GetHeightReal(projPos.x, projPos.z, false);
	}

	return curMapDiameter;
}

float CShadowHandler::GetOrthoProjectedFrustumRadius(CCamera* playerCam, const CMatrix44f& lightViewMat, float3& centerPos) {
	float3 frustumPoints[8];

	#if 0
	{
		float sqRadius = 0.0f;
		projPos = CalcShadowProjectionPos(playerCam, &frustumPoints[0]);

		// calculate radius of the minimally-bounding sphere around projected frustum
		for (unsigned int n = 0; n < 8; n++) {
			sqRadius = std::max(sqRadius, (frustumPoints[n] - projPos).SqLength());
		}

		const float maxMapDiameter = readMap->GetBoundingRadius() * 2.0f;
		const float frustumDiameter = std::sqrt(sqRadius) * 2.0f;

		return (std::min(maxMapDiameter, frustumDiameter));
	}
	#else
	{
		CMatrix44f lightViewCenterMat;
		lightViewCenterMat.SetX(lightViewMat.GetX());
		lightViewCenterMat.SetY(lightViewMat.GetY());
		lightViewCenterMat.SetZ(lightViewMat.GetZ());
		centerPos = CalcShadowProjectionPos(playerCam, &frustumPoints[0]);
		lightViewCenterMat.SetPos(centerPos);

		// find projected width along {x,z}-axes (.x := min, .y := max)
		float2 xbounds = {std::numeric_limits<float>::max(), -std::numeric_limits<float>::max()};
		float2 zbounds = {std::numeric_limits<float>::max(), -std::numeric_limits<float>::max()};

		for (unsigned int n = 0; n < 8; n++) {
			frustumPoints[n] = lightViewCenterMat * frustumPoints[n];

			xbounds.x = std::min(xbounds.x, frustumPoints[n].x);
			xbounds.y = std::max(xbounds.y, frustumPoints[n].x);
			zbounds.x = std::min(zbounds.x, frustumPoints[n].z);
			zbounds.y = std::max(zbounds.y, frustumPoints[n].z);
		}

		// factor in z-bounds to prevent clipping
		return (std::min(readMap->GetBoundingRadius() * 2.0f, std::max(xbounds.y - xbounds.x, zbounds.y - zbounds.x)));
	}
	#endif
}

float3 CShadowHandler::CalcShadowProjectionPos(CCamera* playerCam, float3* frustumPoints)
{
	static constexpr float T1 = 100.0f;
	static constexpr float T2 = 200.0f;

	float3 projPos;
	for (int i = 0; i < 8; ++i)
		frustumPoints[i] = playerCam->GetFrustumVert(i);

	const std::initializer_list<float4> clipPlanes = {
		float4{-UpVector,  (readMap->GetCurrMaxHeight() + T1) },
		float4{ UpVector, -(readMap->GetCurrMinHeight() - T1) },
	};

	for (int i = 0; i < 4; ++i) {
		//near quadrilateral
		ClipRayByPlanes(frustumPoints[4 + i], frustumPoints[i], clipPlanes);
		//far quadrilateral
		ClipRayByPlanes(frustumPoints[i], frustumPoints[4 + i], clipPlanes);

		//hard clamp xz
		frustumPoints[    i].x = std::clamp(frustumPoints[    i].x, -T2, mapDims.mapx * SQUARE_SIZE + T2);
		frustumPoints[    i].z = std::clamp(frustumPoints[    i].z, -T2, mapDims.mapy * SQUARE_SIZE + T2);
		frustumPoints[4 + i].x = std::clamp(frustumPoints[4 + i].x, -T2, mapDims.mapx * SQUARE_SIZE + T2);
		frustumPoints[4 + i].z = std::clamp(frustumPoints[4 + i].z, -T2, mapDims.mapy * SQUARE_SIZE + T2);

		projPos += frustumPoints[i] + frustumPoints[4 + i];
	}

	projPos *= 0.125f;

	return projPos;
}
