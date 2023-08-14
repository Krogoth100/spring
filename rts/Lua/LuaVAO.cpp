#include "LuaVAO.h"

#include "lib/sol2/sol.hpp"

#include "LuaVAOImpl.h"
#include "LuaUtils.h"


/******************************************************************************
 * @module LuaVAO
 *
 * @see rts/Lua/LuaVAO.cpp
******************************************************************************/


bool LuaVAOs::PushEntries(lua_State* L)
{
#if defined(__GNUG__) && defined(_DEBUG)
	const int top = lua_gettop(L);
#endif
	REGISTER_LUA_CFUNC(GetVAO);

	sol::state_view lua(L);
	auto gl = sol::stack::get<sol::table>(L);

	gl.new_usertype<LuaVAOImpl>("VAO",
		sol::constructors<LuaVAOImpl(const LuaVAOImpl::LuaXBOImplSP&, const sol::optional<LuaVAOImpl::LuaXBOImplSP>&, const sol::optional<LuaVAOImpl::LuaXBOImplSP>&)>(),
		"Delete", &LuaVAOImpl::Delete,

		"UpdateUnitBins", &LuaVAOImpl::UpdateUnitBins,
		"UpdateFeatureBins", &LuaVAOImpl::UpdateFeatureBins,
		"SubmitBins", sol::overload(
			sol::resolve<void()>(&LuaVAOImpl::SubmitBins),
			sol::resolve<void(const sol::function)>(&LuaVAOImpl::SubmitBins)
		),

		"SetDrawMode", &LuaVAOImpl::SetDrawMode,
		"Draw", &LuaVAOImpl::Draw,
		"DrawReusedBins", &LuaVAOImpl::DrawReusedBins,

		"ClearSubmission", &LuaVAOImpl::ClearSubmission,
		"AddUnitDefsToSubmission", sol::overload(
			sol::resolve<int(int)>(&LuaVAOImpl::AddUnitDefsToSubmission),
			sol::resolve<int(const sol::stack_table&)>(&LuaVAOImpl::AddUnitDefsToSubmission)
		),
		"AddFeatureDefsToSubmission", sol::overload(
			sol::resolve<int(int)>(&LuaVAOImpl::AddFeatureDefsToSubmission),
			sol::resolve<int(const sol::stack_table&)>(&LuaVAOImpl::AddFeatureDefsToSubmission)
		),
		"RemoveFromSubmission", & LuaVAOImpl::RemoveFromSubmission,
		"Submit", &LuaVAOImpl::Submit
	);

#if defined(__GNUG__) && defined(_DEBUG)
	lua_settop(L, top); //workaround for https://github.com/ThePhD/sol2/issues/1441, remove when fixed
#endif
	return true;
}

LuaVAOs::~LuaVAOs()
{
	for (auto& lva : luaVAOs) {
		if (lva.expired())
			continue; //destroyed already

		lva.lock()->Delete();
	}
	luaVAOs.clear();
}


/***
 *
 * @function gl.GetVAO
 * @treturn nil|VAO the VAO ref on success, else nil
 * @usage
 * local myVAO = gl.GetVAO()
 * if myVAO == nil then Spring.Echo("Failed to get VAO") end
 */
int LuaVAOs::GetVAO(lua_State* L)
{
	if (!LuaVAOImpl::Supported()) {
		LOG_L(L_ERROR, "[LuaVAOs::%s] Important OpenGL extensions are not supported by the system\n  \tGL_ARB_vertex_buffer_object = %d; GL_ARB_vertex_array_object = %d; GL_ARB_instanced_arrays = %d; GL_ARB_draw_elements_base_vertex = %d; GL_ARB_multi_draw_indirect = %d",
			__func__, (GLEW_ARB_vertex_buffer_object), (GLEW_ARB_vertex_array_object), (GLEW_ARB_instanced_arrays), (GLEW_ARB_draw_elements_base_vertex), (GLEW_ARB_multi_draw_indirect)
		);

		return 0;
	}

	return sol::stack::call_lua(L, 1, [L](const LuaVAOImpl::LuaXBOImplSP& luaVBO, const sol::optional<LuaVAOImpl::LuaXBOImplSP>& luaIBO, const sol::optional<LuaVAOImpl::LuaXBOImplSP>& luaSBO) {
		auto& activeVAOs = CLuaHandle::GetActiveVAOs(L);
		return activeVAOs.luaVAOs.emplace_back(std::make_shared<LuaVAOImpl>(luaVBO, luaIBO, luaSBO)).lock();
	});
}
