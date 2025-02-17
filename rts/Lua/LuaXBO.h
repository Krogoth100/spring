/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#pragma once

#include <memory>
#include <vector>

struct lua_State;
class LuaXBOImpl;

class LuaXBOs {
public:
	virtual ~LuaXBOs();
	static int GetXBO(lua_State* L);
	static bool PushEntries(lua_State* L);
	std::vector<std::weak_ptr<LuaXBOImpl>> luaXBOs;
private:
	static bool CheckAndReportSupported(lua_State* L, const unsigned int target);
};