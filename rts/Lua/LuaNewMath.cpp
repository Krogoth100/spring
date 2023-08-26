#include "LuaNewMath.h"

#include "lib/sol2/sol.hpp"

#include "Helpers/Sol.h"
#include <cmath>


/* Lua */
Sol::Number BinLog(Sol::Number val)
{
	return log2(val);
}


bool LuaNewMath::PushEntries(lua_State* L)
{
#if defined(__GNUG__) && defined(_DEBUG)
	const int top = lua_gettop(L);
#endif
	sol::state_view lua(L);
	auto math = sol::stack::get<sol::table>(L);

	math.create_named("PF",
		"BinLog", &BinLog
	);

#if defined(__GNUG__) && defined(_DEBUG)
	lua_settop(L, top); //workaround for https://github.com/ThePhD/sol2/issues/1441, remove when fixed
#endif

	return true;
}