#include "LuaNewSynced.h"

#include "lib/sol2/sol.hpp"

#include "Map/NewUtils.h"
#include "Map/ReadMap.h"
#include "Map/MapDamage.h"
#include "Sim/Misc/SmoothHeightMesh.h"
#include "Helpers/Sol.h"
#include <cmath>

//Graphic stuff for map operations:
#include "Rendering/Textures/Bitmap.h"
#include "Rendering/GL/myGL.h"
#include "Rendering/GL/SubState.h"

using namespace GL::State;


/* Lua */
void LoadHeightMapFromFile(const char* filePath, float heightBase, float heightScale)
{
	SetHeightMapByFile(filePath, heightBase, heightScale);
}

/* Lua */
void SetHeightMapByTexture(GLuint fbo, int fromX, int fromZ, int toX, int toZ)
{
	//!no bounds check yet

	if (mapDamage->Disabled()) return;

	// Reading
	CBitmap heightBitmap;
	{
		auto state = GL::SubState(ReadBuffer(0));

		const int w = toX-fromX+1;
		const int h = toZ-fromZ+1;
		constexpr int pixelFormatSize = 1;
		constexpr GLenum format = GL_RED;
		heightBitmap.Alloc(w, h, pixelFormatSize*sizeof(float));
		glReadPixels(fromX, fromZ, w, h, format, GL_FLOAT, reinterpret_cast<float*>(heightBitmap.GetRawMem()));
	}

	// Applying
	const float* data = reinterpret_cast<const float*>(heightBitmap.GetRawMem());
	float heightMapAmountChanged = 0;
	{
		/*int x,z;

		// Reading
		float height;

		// Applying
		const float oldHeight = readMap->GetCornerHeightMapSynced()[index];
		heightMapAmountChanged += math::fabsf(height-oldHeight);

		const int index = z*mapDims.mapxp1 +x;
		readMap->SetHeight(index, height);*/
	}

	/*if (heightMapAmountChanged) {
		mapDamage->RecalcArea(fromX, toX, fromZ, toZ);
	}*/
}

/* Lua */
void SetHeightMapByTexture(GLuint fbo)
{
    SetHeightMapByTexture(fbo, 0,0, mapDims.mapx,mapDims.mapy);
}

/* Lua */
void UpdateSmoothHeightMesh()
{
    smoothGround.MakeSmoothMesh();
}


bool LuaNewSynced::PushEntries(lua_State* L)
{
#if defined(__GNUG__) && defined(_DEBUG)
	const int top = lua_gettop(L);
#endif
	sol::state_view lua(L);
	auto spring = sol::stack::get<sol::table>(L);

	spring.create_named("PF",
		"LoadHeightMapFromFile", &LoadHeightMapFromFile,
		"SetHeightMapByTexture", sol::overload(
			sol::resolve<void(GLuint)>(&SetHeightMapByTexture),
			sol::resolve<void(GLuint, int,int, int,int)>(&SetHeightMapByTexture)
		),
		"UpdateSmoothHeightMesh", &UpdateSmoothHeightMesh
	);

#if defined(__GNUG__) && defined(_DEBUG)
	lua_settop(L, top); //workaround for https://github.com/ThePhD/sol2/issues/1441, remove when fixed
#endif

	return true;
}