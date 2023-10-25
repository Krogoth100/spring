/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#pragma once

struct lua_State;

class LuaNewUnsynced {
public:
	static bool PushEntries(lua_State* L);
};