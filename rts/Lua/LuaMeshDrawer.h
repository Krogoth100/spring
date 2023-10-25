/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include <memory>
#include <vector>

struct lua_State;
class LuaMeshDrawerImpl;

class LuaMeshDrawers {
public:
	virtual ~LuaMeshDrawers();
	static int GetMeshDrawer(lua_State* L);
	static bool PushEntries(lua_State* L);
	std::vector<std::weak_ptr<LuaMeshDrawerImpl>> luaMeshDrawers;
};