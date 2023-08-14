#include "LuaXBO.h"

#include <unordered_map>
#include <memory>

#include "lib/sol2/sol.hpp"

#include "Rendering/GL/myGL.h"
#include "LuaXBOImpl.h"
#include "LuaHandle.h"
#include "LuaUtils.h"


/******************************************************************************
 * @module LuaXBO
 *
 * @see rts/Lua/LuaXBO.cpp
******************************************************************************/


bool LuaXBOs::PushEntries(lua_State* L)
{
#if defined(__GNUG__) && defined(_DEBUG)
	const int top = lua_gettop(L);
#endif
	REGISTER_LUA_CFUNC(GetXBO);

	sol::state_view lua(L);
	auto gl = sol::stack::get<sol::table>(L);

	gl.new_usertype<LuaXBOImpl>("XBO",
		sol::constructors<LuaXBOImpl(const sol::optional<GLenum>, const sol::optional<GLenum>)>(),
		"Delete", &LuaXBOImpl::Delete,

		"Define", &LuaXBOImpl::Define,
		"Upload", &LuaXBOImpl::Upload,
		"Download", &LuaXBOImpl::Download,
		"Clear", &LuaXBOImpl::Clear,

		"ModelsXBO", &LuaXBOImpl::ModelsXBO,

		"InstanceDataFromUnitDefIDs", sol::overload(
			sol::resolve<size_t(int, int, sol::optional<int>, sol::optional<int>)>(&LuaXBOImpl::InstanceDataFromUnitDefIDs),
			sol::resolve<size_t(const sol::stack_table&, int, sol::optional<int>, sol::optional<int>)>(&LuaXBOImpl::InstanceDataFromUnitDefIDs)
		),
		"InstanceDataFromFeatureDefIDs", sol::overload(
			sol::resolve<size_t(int, int, sol::optional<int>, sol::optional<int>)>(&LuaXBOImpl::InstanceDataFromFeatureDefIDs),
			sol::resolve<size_t(const sol::stack_table&, int, sol::optional<int>, sol::optional<int>)>(&LuaXBOImpl::InstanceDataFromFeatureDefIDs)
		),
		"InstanceDataFromUnitIDs", sol::overload(
			sol::resolve<size_t(int, int, sol::optional<int>)>(&LuaXBOImpl::InstanceDataFromUnitIDs),
			sol::resolve<size_t(const sol::stack_table&, int, sol::optional<int>)>(&LuaXBOImpl::InstanceDataFromUnitIDs)
		),
		"InstanceDataFromFeatureIDs", sol::overload(
			sol::resolve<size_t(int, int, sol::optional<int>)>(&LuaXBOImpl::InstanceDataFromFeatureIDs),
			sol::resolve<size_t(const sol::stack_table&, int, sol::optional<int>)>(&LuaXBOImpl::InstanceDataFromFeatureIDs)
		),
		"MatrixDataFromProjectileIDs", sol::overload(
			sol::resolve<size_t(int, int, sol::optional<int>)>(&LuaXBOImpl::MatrixDataFromProjectileIDs),
			sol::resolve<size_t(const sol::stack_table&, int, sol::optional<int>)>(&LuaXBOImpl::MatrixDataFromProjectileIDs)
		),

		"BindBufferRange", &LuaXBOImpl::BindBufferRange,
		"UnbindBufferRange", &LuaXBOImpl::UnbindBufferRange,

		"DumpDefinition", &LuaXBOImpl::DumpDefinition,
		"GetBufferSize", &LuaXBOImpl::GetBufferSize
	);

#if defined(__GNUG__) && defined(_DEBUG)
	lua_settop(L, top); //workaround for https://github.com/ThePhD/sol2/issues/1441, remove when fixed
#endif

	return true;
}

bool LuaXBOs::CheckAndReportSupported(lua_State* L, const unsigned int target) {
	#define ValStr(arg) { arg, #arg }
	#define ValStr2(arg1, arg2) { arg1, #arg2 }

	static std::unordered_map<GLenum, std::string> bufferEnumToStr = {
		ValStr(GL_ARRAY_BUFFER),
		ValStr(GL_ELEMENT_ARRAY_BUFFER),
		ValStr(GL_UNIFORM_BUFFER),
		ValStr(GL_SHADER_STORAGE_BUFFER),
	};

	static std::unordered_map<GLenum, std::string> bufferEnumToExtStr = {
		ValStr2(GL_ARRAY_BUFFER, ARB_vertex_buffer_object),
		ValStr2(GL_ELEMENT_ARRAY_BUFFER, ARB_vertex_buffer_object),
		ValStr2(GL_UNIFORM_BUFFER, ARB_uniform_buffer_object),
		ValStr2(GL_SHADER_STORAGE_BUFFER, ARB_shader_storage_buffer_object),
	};

	if (bufferEnumToStr.find(target) == bufferEnumToStr.cend()) {
		LOG_L(L_ERROR, "[LuaXBOs:%s]: Supplied invalid OpenGL buffer type [%u]", __func__, target);
		return false;
	}

	if (!LuaXBOImpl::Supported(target)) {
		LOG_L(L_ERROR, "[LuaXBOs:%s]: important OpenGL extension %s is not supported for buffer type %s", __func__, bufferEnumToExtStr[target].c_str(), bufferEnumToStr[target].c_str());
		return false;
	}

	return true;

	#undef ValStr
	#undef ValStr2
}

LuaXBOs::~LuaXBOs()
{
	for (auto lvb : luaXBOs) {
		if (lvb.expired())
			continue; //destroyed already

		lvb.lock()->Delete();
	}
	luaXBOs.clear();
}


/***
 *
 * @function gl.GetXBO
 * @number[opt=GL.ARRAY_BUFFER] bufferType one of [`GL.ARRAY_BUFFER`,
 * `GL.ELEMENT_ARRAY_BUFFER`, `GL.UNIFORM_BUFFER`, `GL.SHADER_STORAGE_BUFFER`].
 *
 * Defaults to `GL.ARRAY_BUFFER`, which you should use for vertex data, and
 * `GL.ELEMENT_ARRAY_BUFFER` should be used for vertex indices.
 * @bool[opt=true] freqUpdated whether should be updated frequently, when false
 * will be updated only once
 * @treturn nil|XBO the XBO ref on success, nil if not supported/or other error
 * @see GL.OpenGL_Buffer_Types
 * @usage
 * local myXBO = gl.GetXBO()
 * if myXBO == nil then Spring.Echo("Failed to get XBO") end
 */
int LuaXBOs::GetXBO(lua_State* L)
{
	unsigned int target = luaL_optint(L, 1, GL_ARRAY_BUFFER);
	if (!LuaXBOs::CheckAndReportSupported(L, target))
		return 0;

	return sol::stack::call_lua(L, 1, [L](const sol::optional<GLenum> defTargetOpt, const sol::optional<bool> freqUpdatedOpt) {
		auto& activeXBOs = CLuaHandle::GetActiveXBOs(L);
		return activeXBOs.luaXBOs.emplace_back(std::make_shared<LuaXBOImpl>(defTargetOpt, freqUpdatedOpt)).lock();
	});
}
