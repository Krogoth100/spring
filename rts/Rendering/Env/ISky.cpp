/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */


#include "ISky.h"
#include "NullSky.h"
#include "Game/Camera.h"
#include "Game/TraceRay.h"
#include "Map/MapInfo.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/GL/myGL.h"
#include "System/Config/ConfigHandler.h"
#include "System/Exceptions.h"
#include "System/SafeUtil.h"
#include "System/Log/ILog.h"

CONFIG(bool, AdvSky).deprecated(true);

ISky::ISky()
	: skyColor(mapInfo->atmosphere.skyColor)
	, sunColor(mapInfo->atmosphere.sunColor)
	, cloudColor(mapInfo->atmosphere.cloudColor)
	, fogColor(mapInfo->atmosphere.fogColor)
	, fogStart(mapInfo->atmosphere.fogStart)
	, fogEnd(mapInfo->atmosphere.fogEnd)
	, cloudDensity(mapInfo->atmosphere.cloudDensity)
	, skyLight(nullptr)
	, wireFrameMode(false)
{
	skyLight = new ISkyLight();
}

ISky::~ISky()
{
	spring::SafeDelete(skyLight);
}



void ISky::SetupFog() {

	if (globalRendering->drawFog) {
		glEnable(GL_FOG);
	} else {
		glDisable(GL_FOG);
	}

	glFogfv(GL_FOG_COLOR, fogColor);
	glFogi(GL_FOG_MODE,   GL_LINEAR);
	glFogf(GL_FOG_START,  camera->GetFarPlaneDist() * fogStart);
	glFogf(GL_FOG_END,    camera->GetFarPlaneDist() * fogEnd);
	glFogf(GL_FOG_DENSITY, 1.0f);
}

void ISky::SetSky()
{
	sky = std::make_unique<CNullSky>();
}

bool ISky::SunVisible(const float3 pos) const {
	const CUnit* hitUnit = nullptr;
	const CFeature* hitFeature = nullptr;

	// cast a ray *toward* the sun from <pos>
	// sun is visible if no terrain blocks it
	const float3& sunDir = skyLight->GetLightDir();
	const float sunDist = TraceRay::GuiTraceRay(pos, sunDir, camera->GetFarPlaneDist(), nullptr, hitUnit, hitFeature, false, true, false);

	return (sunDist < 0.0f || sunDist >= camera->GetFarPlaneDist());
}

