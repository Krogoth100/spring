#include "LuaMeshDrawer.h"

#include "lib/sol2/sol.hpp"

#include "LuaMeshDrawerImpl.h"
#include "LuaUtils.h"


/******************************************************************************
 * @module LuaMeshDrawer
 *
 * @see rts/Lua/LuaMeshDrawer.cpp
******************************************************************************/


bool LuaMeshDrawers::PushEntries(lua_State* L)
{
#if defined(__GNUG__) && defined(_DEBUG)
	const int top = lua_gettop(L);
#endif
	REGISTER_LUA_CFUNC(GetMeshDrawer);

	sol::state_view lua(L);
	auto gl = sol::stack::get<sol::table>(L);

	gl.new_usertype<LuaMeshDrawerImpl>("MeshDrawer",
		sol::constructors<LuaMeshDrawerImpl(const LuaMeshDrawerImpl::LuaXBOImplSP&, const sol::optional<LuaMeshDrawerImpl::LuaXBOImplSP>&, const sol::optional<LuaMeshDrawerImpl::LuaXBOImplSP>&)>(),
		"Delete", &LuaMeshDrawerImpl::Delete,

		"UpdateUnitBins", &LuaMeshDrawerImpl::UpdateUnitBins,
		"UpdateFeatureBins", &LuaMeshDrawerImpl::UpdateFeatureBins,
		"SubmitBins", sol::overload(
			sol::resolve<void()>(&LuaMeshDrawerImpl::SubmitBins),
			sol::resolve<void(const sol::function)>(&LuaMeshDrawerImpl::SubmitBins)
		),

		"SetDrawMode", &LuaMeshDrawerImpl::SetDrawMode,
		"Draw", &LuaMeshDrawerImpl::Draw,
		"DrawReusedBins", &LuaMeshDrawerImpl::DrawReusedBins,

		"ClearSubmission", &LuaMeshDrawerImpl::ClearSubmission,
		"AddUnitDefsToSubmission", sol::overload(
			sol::resolve<int(int)>(&LuaMeshDrawerImpl::AddUnitDefsToSubmission),
			sol::resolve<int(const sol::stack_table&)>(&LuaMeshDrawerImpl::AddUnitDefsToSubmission)
		),
		"AddFeatureDefsToSubmission", sol::overload(
			sol::resolve<int(int)>(&LuaMeshDrawerImpl::AddFeatureDefsToSubmission),
			sol::resolve<int(const sol::stack_table&)>(&LuaMeshDrawerImpl::AddFeatureDefsToSubmission)
		),
		"RemoveFromSubmission", & LuaMeshDrawerImpl::RemoveFromSubmission,
		"Submit", &LuaMeshDrawerImpl::Submit
	);

#if defined(__GNUG__) && defined(_DEBUG)
	lua_settop(L, top); //workaround for https://github.com/ThePhD/sol2/issues/1441, remove when fixed
#endif
	return true;
}

LuaMeshDrawers::~LuaMeshDrawers()
{
	for (auto& luaMeshDrawer : luaMeshDrawers) {
		if (luaMeshDrawer.expired())
			continue; //destroyed already

		luaMeshDrawer.lock()->Delete();
	}
	luaMeshDrawers.clear();
}


/***
 *
 * @function gl.GetMeshDrawer
 * @treturn nil|MeshDrawer the MeshDrawer ref on success, else nil
 * @usage
 * local myMeshDrawer = gl.GetMeshDrawer()
 * if myMeshDrawer == nil then Spring.Echo("Failed to get MeshDrawer") end
 */
int LuaMeshDrawers::GetMeshDrawer(lua_State* L)
{
	if (!LuaMeshDrawerImpl::Supported()) {
		LOG_L(L_ERROR, "[LuaMeshDrawers::%s] Important OpenGL extensions are not supported by the system\n  \tGL_ARB_vertex_buffer_object = %d; GL_ARB_vertex_array_object = %d; GL_ARB_instanced_arrays = %d; GL_ARB_draw_elements_base_vertex = %d; GL_ARB_multi_draw_indirect = %d",
			__func__, (GLEW_ARB_vertex_buffer_object), (GLEW_ARB_vertex_array_object), (GLEW_ARB_instanced_arrays), (GLEW_ARB_draw_elements_base_vertex), (GLEW_ARB_multi_draw_indirect)
		);

		return 0;
	}

	return sol::stack::call_lua(L, 1, [L](const LuaMeshDrawerImpl::LuaXBOImplSP& luaVBO, const sol::optional<LuaMeshDrawerImpl::LuaXBOImplSP>& luaIBO, const sol::optional<LuaMeshDrawerImpl::LuaXBOImplSP>& luaSBO) {
		auto& activeMeshDrawers = CLuaHandle::GetActiveMeshDrawers(L);
		return activeMeshDrawers.luaMeshDrawers.emplace_back(std::make_shared<LuaMeshDrawerImpl>(luaVBO, luaIBO, luaSBO)).lock();
	});
}
