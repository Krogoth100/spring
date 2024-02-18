#include "LuaNewUnsynced.h"

#include "lib/sol2/sol.hpp"

#include "Rendering/GL/myGL.h"
#include "Helpers/Sol.h"
#include <cmath>


bool LuaNewUnsynced::PushEntries(lua_State* L)
{
#if defined(__GNUG__) && defined(_DEBUG)
	const int top = lua_gettop(L);
#endif
	sol::state_view lua(L);
	auto spring = sol::stack::get<sol::table>(L);

	/*spring.create_named("PF",
		...
	);*/

#if defined(__GNUG__) && defined(_DEBUG)
	lua_settop(L, top); //workaround for https://github.com/ThePhD/sol2/issues/1441, remove when fixed
#endif

	return true;
}