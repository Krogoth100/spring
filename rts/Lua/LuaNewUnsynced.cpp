#include "LuaNewUnsynced.h"

#include "lib/sol2/sol.hpp"

#include "Rendering/GL/myGL.h"
#include "Helpers/Sol.h"
#include <cmath>


/* Lua */
void SetHeightMapFromTexture(GLuint texture, GLint x, GLint y, GLsizei w, GLsizei h)
{
}

/* Lua */
void SetHeightMapFromTexture(GLuint texture)
{
    //SetHeightMapFromTexture(texture, 1,1, );
}


bool LuaNewUnsynced::PushEntries(lua_State* L)
{
#if defined(__GNUG__) && defined(_DEBUG)
	const int top = lua_gettop(L);
#endif
	sol::state_view lua(L);
	auto spring = sol::stack::get<sol::table>(L);

	spring.create_named("PF",
		"SetHeightMapFromTexture", sol::overload(
			sol::resolve<void(GLuint)>(&SetHeightMapFromTexture),
			sol::resolve<void(GLuint, GLint,GLint, GLsizei,GLsizei)>(&SetHeightMapFromTexture)
		)
	);

#if defined(__GNUG__) && defined(_DEBUG)
	lua_settop(L, top); //workaround for https://github.com/ThePhD/sol2/issues/1441, remove when fixed
#endif

	return true;
}