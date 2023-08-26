#include "LuaNewGL.h"

#include "lib/sol2/sol.hpp"

#include "Rendering/GL/myGL.h"
#include "Rendering/GL/SubState.h"
#include "Rendering/GL/XBO.h"
#include "Rendering/Textures/TextureFormat.h"
#include "Rendering/Models/3DModelVAO.h"
#include "LuaHandle.h"
#include "LuaUtils.h"
#include "System/SafeUtil.h"
#include "System/StringHash.h"
#include "System/Log/ILog.h"
#include "Helpers/Sol.h"

using namespace GL::State;


///////////////////////////////////////////////////////////////////
//
//  Framebuffer


namespace Impl {
	template<class Type, auto glClearBufferFuncPtrPtr>
	inline void ClearBuffer(GLenum bufferType, GLint drawBuffer, sol::optional<float> r, sol::optional<float> g, sol::optional<float> b, sol::optional<float> a) {
		Type values[4];
		values[0] = spring::SafeCast<Type>(r.value_or(0));
		values[1] = spring::SafeCast<Type>(g.value_or(0));
		values[2] = spring::SafeCast<Type>(b.value_or(0));
		values[3] = spring::SafeCast<Type>(a.value_or(0));
		(*glClearBufferFuncPtrPtr)(bufferType, drawBuffer, values);
	}

	void ClearBuffer(GLenum attachmentInternalFormat, GLenum bufferType, GLenum drawBuffer,
		sol::optional<float> r, sol::optional<float> g, sol::optional<float> b, sol::optional<float> a)
	{
		switch(attachmentInternalFormat) {
		case GL_R8UI:
		case GL_RG8UI:
		case GL_RGBA8UI:
		case GL_R16UI:
		case GL_RG16UI:
		case GL_RGBA16UI:
		case GL_RGB10_A2UI:
		case GL_R32UI:
		case GL_RG32UI:
		case GL_RGBA32UI:
			Impl::ClearBuffer<GLuint, &glClearBufferuiv>(bufferType, drawBuffer, r,g,b,a);
			break;
		case GL_R8I:
		case GL_RG8I:
		case GL_RGBA8I:
		case GL_R16I:
		case GL_RG16I:
		case GL_RGBA16I:
		case GL_R32I:
		case GL_RG32I:
		case GL_RGBA32I:
			Impl::ClearBuffer<GLint, &glClearBufferiv>(bufferType, drawBuffer, r,g,b,a);
			break;
		default:
			Impl::ClearBuffer<GLfloat, &glClearBufferfv>(bufferType, drawBuffer, r,g,b,a);
			break;
		}
	}
}

/* Lua */
void ClearBuffer(sol::optional<GLenum> slot_, sol::optional<float> r, sol::optional<float> g, sol::optional<float> b, sol::optional<float> a, sol::this_state lua)
{
	const GLenum slot = slot_.value_or(1);
	assert(slot >= 1);

	//CheckDrawingEnabled(lua, __func__);

	const GLenum bufferType = GL_COLOR;
	const GLenum attachment = GL_COLOR_ATTACHMENT0+slot-1;
	const GLenum drawBuffer = slot-1;

	const auto activeLuaFBO = CLuaHandle::GetActiveFBOs(lua).GetActiveDrawFBO();
	const GLenum attachmentInternalFormat = activeLuaFBO? activeLuaFBO->GetAttachmentFormat(attachment) : GL_RGBA8;
	// if not a Lua FBO, it may be default framebuffer; proceed with a typical format

	Impl::ClearBuffer(attachmentInternalFormat, bufferType, drawBuffer, r,g,b,a);
}

/* Lua */
void ClearBuffer(const char* slot, sol::optional<float> r, sol::optional<float> g, sol::optional<float> b, sol::optional<float> a, sol::this_state lua)
{
	assert(hashString(slot) == hashString("depth") || hashString(slot) == hashString("stencil"));

	//CheckDrawingEnabled(lua, __func__);

	const bool isDepth = (slot[0] == 'd');

	const GLenum bufferType = isDepth? GL_DEPTH : GL_STENCIL;
	const GLenum attachment = isDepth? GL_DEPTH_ATTACHMENT : GL_STENCIL_ATTACHMENT;
	constexpr GLenum drawBuffer = 0;

	const auto activeLuaFBO = CLuaHandle::GetActiveFBOs(lua).GetActiveDrawFBO();
	const GLenum attachmentInternalFormat = activeLuaFBO? activeLuaFBO->GetAttachmentFormat(attachment) : GL_RGBA8;
	// if not a Lua FBO, it may be default framebuffer; proceed with a typical format

	Impl::ClearBuffer(attachmentInternalFormat, bufferType, drawBuffer, r,g,b,a);
}


namespace Impl {
	template<class Type>
	inline Sol::MultipleNumbers<4> ReadPixelResult(GLint x, GLint y, GLenum format, GLenum readType) {
		Type values[4];
		glReadPixels(x, y, 1, 1, format, readType, values);
		return Sol::MultipleNumbers<4>(values[0],values[1],values[2],values[3]);
	}

	Sol::MultipleNumbers<4> ReadAttachmentPixel(GLenum attachment, GLint x, GLint y, sol::this_state& lua)
	{
		const auto activeLuaFBO = CLuaHandle::GetActiveFBOs(lua).GetActiveReadFBO();
		assert(activeLuaFBO);
		const GLenum internalFormat = activeLuaFBO->GetAttachmentFormat(attachment);
		const GLenum format = GL::GetInternalFormatDataFormat(internalFormat);
		const GLenum readType = GL::GetInternalFormatUserType(internalFormat);

		auto state = GL::SubState(ReadBuffer(attachment));

		switch(readType) {
		case GL_FLOAT:          return Impl::ReadPixelResult<GLfloat> (x, y, format, readType);
		case GL_HALF_FLOAT:     return Impl::ReadPixelResult<GLhalf>  (x, y, format, readType);
		case GL_INT:            return Impl::ReadPixelResult<GLint>   (x, y, format, readType);
		case GL_SHORT:          return Impl::ReadPixelResult<GLshort> (x, y, format, readType);
		case GL_BYTE:           return Impl::ReadPixelResult<GLbyte>  (x, y, format, readType);
		case GL_UNSIGNED_INT:   return Impl::ReadPixelResult<GLuint>  (x, y, format, readType);
		case GL_UNSIGNED_SHORT: return Impl::ReadPixelResult<GLushort>(x, y, format, readType);
		case GL_UNSIGNED_BYTE:  return Impl::ReadPixelResult<GLubyte> (x, y, format, readType);
		}

		return Sol::MultipleNumbers<4>(0,0,0,0);
	}
}

/* Lua */
Sol::MultipleNumbers<4> ReadAttachmentPixel(sol::optional<GLenum> slot_, GLint x, GLint y, sol::this_state lua)
{
	const GLenum slot = slot_.value_or(1);
	assert(slot >= 1);

	return Impl::ReadAttachmentPixel(GL_COLOR_ATTACHMENT0 +slot -1, x,y, lua);
}

/* Lua */
Sol::MultipleNumbers<4> ReadAttachmentPixel(const char* slot, GLint x, GLint y, sol::this_state lua)
{
	assert(hashString(slot) == hashString("depth"));

	return Impl::ReadAttachmentPixel(GL_DEPTH_ATTACHMENT, x,y, lua);
}


///////////////////////////////////////////////////////////////////
//
//  Mesh Buffers


/* Lua */
void BindEngineModelMeshBuffers(GLuint vboBindingPoint, GLuint iboBindingPoint)
{
	const XBO* vbo = S3DModelVAO::GetInstance().GetVertVBO();
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, vboBindingPoint, vbo->GetId());
	const XBO* ibo = S3DModelVAO::GetInstance().GetIndxVBO();
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, iboBindingPoint, ibo->GetId());
}

/* Lua */
void UnbindEngineModelMeshBuffers(GLuint vboBindingPoint, GLuint iboBindingPoint)
{
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, vboBindingPoint, 0);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, iboBindingPoint, 0);
}

/* Lua */
sol::optional<int> GetUnitDefModelIndexStart(int unitDefID)
{
	const UnitDef* unitDef = unitDefHandler->GetUnitDefByID(unitDefID);
	if (!unitDef) return std::nullopt;

	const S3DModel* model = unitDef->LoadModel();
	if (!model) return std::nullopt;

	return model->indxStart;
}

/* Lua */
sol::optional<int> GetFeatureDefModelIndexStart(int featureDefID)
{
	const FeatureDef* featureDef = featureDefHandler->GetFeatureDefByID(featureDefID);
	if (!featureDef) return std::nullopt;

	const S3DModel* model = featureDef->LoadModel();
	if (!model) return std::nullopt;

	return model->indxStart;
}


bool LuaNewGL::PushEntries(lua_State* L)
{
#if defined(__GNUG__) && defined(_DEBUG)
	const int top = lua_gettop(L);
#endif
	sol::state_view lua(L);
	auto gl = sol::stack::get<sol::table>(L);

	gl.create_named("PF",
		"ClearBuffer", sol::overload(
			sol::resolve<void(sol::optional<GLenum>, sol::optional<float>,sol::optional<float>,sol::optional<float>,sol::optional<float>, sol::this_state)>(&ClearBuffer),
			sol::resolve<void(const char*, sol::optional<float>,sol::optional<float>,sol::optional<float>,sol::optional<float>, sol::this_state)>(&ClearBuffer)
		),
		"ReadAttachmentPixel", sol::overload(
			sol::resolve<Sol::MultipleNumbers<4>(sol::optional<GLenum>, GLint,GLint, sol::this_state)>(&ReadAttachmentPixel),
			sol::resolve<Sol::MultipleNumbers<4>(const char*, GLint,GLint, sol::this_state)>(&ReadAttachmentPixel)
		),

		"BindEngineModelMeshBuffers", &BindEngineModelMeshBuffers,
		"UnbindEngineModelMeshBuffers", &UnbindEngineModelMeshBuffers,
		"GetUnitDefModelIndexStart", &GetUnitDefModelIndexStart,
		"GetFeatureDefModelIndexStart", &GetFeatureDefModelIndexStart
	);

#if defined(__GNUG__) && defined(_DEBUG)
	lua_settop(L, top); //workaround for https://github.com/ThePhD/sol2/issues/1441, remove when fixed
#endif

	return true;
}