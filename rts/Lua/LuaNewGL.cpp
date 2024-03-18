#include "LuaNewGL.h"

#include "lib/sol2/sol.hpp"

#include "Rendering/GL/myGL.h"
#include "Rendering/GL/XBO.h"
#include "Rendering/Textures/TextureFormat.h"
#include "Rendering/Models/3DModelVAO.h"
#include "LuaHandle.h"
#include "LuaUtils.h"
#include "System/SafeUtil.h"
#include "System/StringHash.h"
#include "System/Log/ILog.h"
#include "Helpers/Sol.h"

#include <utility>


///////////////////////////////////////////////////////////////////
//
//  Framebuffer


namespace Impl {
	template<class Type, auto glClearBufferFuncPtrPtr>
	inline void ClearBuffer(GLenum bufferType, GLint drawBuffer, SOL_OPTIONAL_4(Sol::Number, r,g,b,a)) {
		Type values[4];
		values[0] = spring::SafeCast<Type>(r.value_or(0));
		values[1] = spring::SafeCast<Type>(g.value_or(0));
		values[2] = spring::SafeCast<Type>(b.value_or(0));
		values[3] = spring::SafeCast<Type>(a.value_or(0));
		(*glClearBufferFuncPtrPtr)(bufferType, drawBuffer, values);
	}

	void ClearBuffer(GLenum attachment, GLenum bufferType, GLenum drawBuffer, SOL_OPTIONAL_4(Sol::Number, r,g,b,a), sol::this_state& lua)
	{
		const auto activeLuaFBO = CLuaHandle::GetActiveFBOs(lua).GetActiveDrawFBO();
		const GLenum attachmentInternalFormat = activeLuaFBO? activeLuaFBO->GetAttachmentFormat(attachment) : GL_RGBA8;
		// if not a Lua FBO, it may be default framebuffer; proceed with a typical format

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
void ClearBuffer(sol::optional<GLenum> slot_, SOL_OPTIONAL_4(Sol::Number, r,g,b,a), sol::this_state lua)
{
	const GLenum slot = slot_.value_or(1);
	assert(slot >= 1);

	//CheckDrawingEnabled(lua, __func__);

	Impl::ClearBuffer(GL_COLOR_ATTACHMENT0+slot-1, GL_COLOR, slot-1, r,g,b,a, lua);
}

/* Lua */
void ClearBuffer(const char* slot, SOL_OPTIONAL_4(Sol::Number, r,g,b,a), sol::this_state lua)
{
	assert(hashString(slot) == hashString("depth") || hashString(slot) == hashString("stencil"));
	const bool isDepth = (slot[0] == 'd');

	//CheckDrawingEnabled(lua, __func__);

	Impl::ClearBuffer(isDepth? GL_DEPTH_ATTACHMENT : GL_STENCIL_ATTACHMENT, isDepth? GL_DEPTH : GL_STENCIL, 0, r,g,b,a, lua);
}

/* Lua */
void InvalidateFramebuffer(sol::this_state lua)
{
	const auto activeLuaFBO = CLuaHandle::GetActiveFBOs(lua).GetActiveDrawFBO();
	glInvalidateFramebuffer(activeLuaFBO->target, activeLuaFBO->attachmentsN, activeLuaFBO->GetAttachments());
}


///////////////////////////////////////////////////////////////////
//
//  Mesh Buffers


namespace Impl {
	std::optional<std::pair<GLuint, GLuint>> EngineModelMeshBufferBindingPoints;
}

/* Lua */
void UnbindEngineModelMeshBuffers()
{
	if (!Impl::EngineModelMeshBufferBindingPoints.has_value()) return;
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Impl::EngineModelMeshBufferBindingPoints->first, 0);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Impl::EngineModelMeshBufferBindingPoints->second, 0);
	Impl::EngineModelMeshBufferBindingPoints = std::nullopt;
}

/* Lua */
void BindEngineModelMeshBuffers(GLuint vboBindingPoint, GLuint iboBindingPoint)
{
	if (Impl::EngineModelMeshBufferBindingPoints.has_value()) UnbindEngineModelMeshBuffers();
	const XBO* vbo = S3DModelVAO::GetInstance().GetVertVBO();
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, vboBindingPoint, vbo->GetId());
	const XBO* ibo = S3DModelVAO::GetInstance().GetIndxVBO();
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, iboBindingPoint, ibo->GetId());
	Impl::EngineModelMeshBufferBindingPoints = std::make_pair(vboBindingPoint, iboBindingPoint);
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


///////////////////////////////////////////////////////////////////
//
//  Write Control


/* Lua */
void DepthWrite(sol::optional<bool> enabled)
{
	glDepthMask((GLboolean) enabled.value_or(true));
}

/* Lua */
void ColorWrite(sol::optional<bool> enabled)
{
	GLboolean glEnabled = (GLboolean) enabled.value_or(true);
	glColorMask(glEnabled, glEnabled, glEnabled, glEnabled);
}

/* Lua */
void ColorWrite(GLboolean c1, SOL_OPTIONAL_3(GLboolean, c2,c3,c4))
{
	GLboolean mask[5] {GL_FALSE};
		mask[c1]             = GL_TRUE;
		mask[c2.value_or(0)] = GL_TRUE;
		mask[c3.value_or(0)] = GL_TRUE;
		mask[c4.value_or(0)] = GL_TRUE;
	glColorMask(mask[1], mask[2], mask[3], mask[4]);
}

/* Lua */
void SlotColorWrite(GLuint slot, sol::optional<bool> enabled)
{
	GLboolean glEnabled = (GLboolean) enabled.value_or(true);
	glColorMaski(slot-1, glEnabled, glEnabled, glEnabled, glEnabled);
}

/* Lua */
void SlotColorWrite(GLuint slot, GLboolean c1, SOL_OPTIONAL_3(GLboolean, c2,c3,c4))
{
	GLboolean mask[5] {GL_FALSE};
		mask[c1]             = GL_TRUE;
		mask[c2.value_or(0)] = GL_TRUE;
		mask[c3.value_or(0)] = GL_TRUE;
		mask[c4.value_or(0)] = GL_TRUE;
	glColorMaski(slot-1, mask[1], mask[2], mask[3], mask[4]);
}


///////////////////////////////////////////////////////////////////
//
//  Textures / Samplers


/* Lua */
void InvalidateTexContents(GLuint textureId, GLint mip)
{
	glInvalidateTexImage(textureId, mip);
}

namespace Impl {
	template<class Type>
	inline void ClearTexture(GLuint textureId, GLint mip, GLenum format, GLenum dataType, SOL_OPTIONAL_4(Sol::Number, r,g,b,a)) {
		Type values[4];
		values[0] = spring::SafeCast<Type>(r.value_or(0));
		values[1] = spring::SafeCast<Type>(g.value_or(0));
		values[2] = spring::SafeCast<Type>(b.value_or(0));
		values[3] = spring::SafeCast<Type>(a.value_or(0));
		glClearTexImage(textureId, mip, format, dataType, values);
	}
}

/* Lua */
void ClearTexture(GLuint textureId, GLenum internalFormat, GLint mip, SOL_OPTIONAL_4(Sol::Number, r,g,b,a))
{
	const GLenum format = GL::GetInternalFormatDataFormat(internalFormat);
	const GLenum dataType = GL::GetInternalFormatUserType(internalFormat);

	switch(dataType) {
	case GL_FLOAT:          Impl::ClearTexture<GLfloat> (textureId, mip, format, dataType, r,g,b,a); break;
	case GL_HALF_FLOAT:     Impl::ClearTexture<GLhalf>  (textureId, mip, format, dataType, r,g,b,a); break;
	case GL_INT:            Impl::ClearTexture<GLint>   (textureId, mip, format, dataType, r,g,b,a); break;
	case GL_SHORT:          Impl::ClearTexture<GLshort> (textureId, mip, format, dataType, r,g,b,a); break;
	case GL_BYTE:           Impl::ClearTexture<GLbyte>  (textureId, mip, format, dataType, r,g,b,a); break;
	case GL_UNSIGNED_INT:   Impl::ClearTexture<GLuint>  (textureId, mip, format, dataType, r,g,b,a); break;
	case GL_UNSIGNED_SHORT: Impl::ClearTexture<GLushort>(textureId, mip, format, dataType, r,g,b,a); break;
	case GL_UNSIGNED_BYTE:  Impl::ClearTexture<GLubyte> (textureId, mip, format, dataType, r,g,b,a); break;
	}
}

/* Lua */
void CopyTexture(GLuint fromTextureId, GLuint toTextureId, GLenum fromTarget, GLenum toTarget, GLint fromMip, GLint toMip, GLsizei width, GLsizei height, GLsizei depth)
{
	glCopyImageSubData(
		fromTextureId, fromTarget, fromMip, 0, 0, 0,
		toTextureId, toTarget, toMip, 0, 0, 0,
		width, height, depth);
}

/* Lua */
void GenTextureMips(GLuint textureId)
{
	glGenerateTextureMipmap(textureId);
}

/* Lua */
void BindSampler(GLenum slot, GLuint textureId)
{
	glBindTextureUnit(slot, textureId);
}

namespace Impl {
	template<class Type>
	inline Sol::MultipleNumbers<4> ReadTexelResult(GLuint textureId, GLint mip, GLint x, GLint y, GLint z, GLenum format, GLenum readType) {
		Type values[4];
		glGetTextureSubImage(textureId, mip, x,y,z, 1,1,1, format, readType, sizeof(Type)*4, values);
		return Sol::MultipleNumbers<4>(values[0],values[1],values[2],values[3]);
	}
}

/* Lua */
Sol::MultipleNumbers<4> ReadTexel(GLuint textureId, GLenum internalFormat, GLint mip, GLint x, GLint y, sol::optional<GLint> z_)
{
	const GLint z = z_.value_or(0);
	const GLenum format = GL::GetInternalFormatDataFormat(internalFormat);
	const GLenum readType = GL::GetInternalFormatUserType(internalFormat);

	switch(readType) {
	case GL_FLOAT:          return Impl::ReadTexelResult<GLfloat> (textureId, mip, x, y, z, format, readType);
	case GL_HALF_FLOAT:     return Impl::ReadTexelResult<GLhalf>  (textureId, mip, x, y, z, format, readType);
	case GL_INT:            return Impl::ReadTexelResult<GLint>   (textureId, mip, x, y, z, format, readType);
	case GL_SHORT:          return Impl::ReadTexelResult<GLshort> (textureId, mip, x, y, z, format, readType);
	case GL_BYTE:           return Impl::ReadTexelResult<GLbyte>  (textureId, mip, x, y, z, format, readType);
	case GL_UNSIGNED_INT:   return Impl::ReadTexelResult<GLuint>  (textureId, mip, x, y, z, format, readType);
	case GL_UNSIGNED_SHORT: return Impl::ReadTexelResult<GLushort>(textureId, mip, x, y, z, format, readType);
	case GL_UNSIGNED_BYTE:  return Impl::ReadTexelResult<GLubyte> (textureId, mip, x, y, z, format, readType);
	}

	return Sol::MultipleNumbers<4>(0,0,0,0);
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
			sol::resolve<void(sol::optional<GLenum>, SOL_OPTIONAL_TYPE_4(Sol::Number), sol::this_state)>(&ClearBuffer),
			sol::resolve<void(const char*, SOL_OPTIONAL_TYPE_4(Sol::Number), sol::this_state)>(&ClearBuffer)
		),
		"InvalidateFramebuffer", &InvalidateFramebuffer,

		"BindEngineModelMeshBuffers", &BindEngineModelMeshBuffers,
		"UnbindEngineModelMeshBuffers", &UnbindEngineModelMeshBuffers,
		"GetUnitDefModelIndexStart", &GetUnitDefModelIndexStart,
		"GetFeatureDefModelIndexStart", &GetFeatureDefModelIndexStart,

		"DepthWrite", &DepthWrite,
		"ColorWrite", sol::overload(
			sol::resolve<void(sol::optional<bool>)>(&ColorWrite),
			sol::resolve<void(GLboolean, SOL_OPTIONAL_TYPE_3(GLboolean))>(&ColorWrite)
		),
		"SlotColorWrite", sol::overload(
			sol::resolve<void(GLuint, sol::optional<bool>)>(&SlotColorWrite),
			sol::resolve<void(GLuint, GLboolean, SOL_OPTIONAL_TYPE_3(GLboolean))>(&SlotColorWrite)
		),

		"InvalidateTexContents", &InvalidateTexContents,
		"ClearTexture", &ClearTexture,
		"CopyTexture", &CopyTexture,
		"GenTextureMips", &GenTextureMips,
		"BindSampler", &BindSampler,
		"ReadTexel", &ReadTexel
	);

#if defined(__GNUG__) && defined(_DEBUG)
	lua_settop(L, top); //workaround for https://github.com/ThePhD/sol2/issues/1441, remove when fixed
#endif

	return true;
}