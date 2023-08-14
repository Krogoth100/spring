/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#pragma once

#include <map>
#include <vector>
#include <string>
#include <algorithm>

#include "lib/lua/include/lua.h" //for lua_Number
#include "lib/sol2/forward.hpp"

#include "Rendering/GL/myGL.h"
#include "Rendering/GL/XBO.h"
#include "Rendering/Models/3DModelVAO.h"

class LuaVAOImpl;

class LuaXBOImpl {
public:
	LuaXBOImpl(const sol::optional<GLenum> defTarget, const sol::optional<GLenum> usageHint);

	LuaXBOImpl(const LuaXBOImpl&) = delete;
	LuaXBOImpl(LuaXBOImpl&&) = default;

	~LuaXBOImpl();
	void Delete();

	void Define(const int elementsCount, const sol::optional<sol::object> attribDefArgOpt);
	std::tuple<uint32_t, uint32_t, uint32_t> GetBufferSize();

	size_t Upload(const sol::stack_table& luaTblData, sol::optional<int> attribIdxOpt, sol::optional<int> elemOffsetOpt, sol::optional<int> luaStartIndexOpt, sol::optional<int> luaFinishIndexOpt);
	sol::as_table_t<std::vector<lua_Number>> Download(sol::optional<int> attribIdxOpt, sol::optional<int> elemOffsetOpt, sol::optional<int> elemCountOpt, sol::optional<bool> forceGPUReadOpt);
	void Clear();

	size_t ModelsXBO();

	size_t InstanceDataFromUnitDefIDs(int id, int attrID, sol::optional<int> teamIdOpt, sol::optional<int> elemOffsetOpt);
	size_t InstanceDataFromUnitDefIDs(const sol::stack_table& ids, int attrID, sol::optional<int> teamIdOpt, sol::optional<int> elemOffsetOpt);
	size_t InstanceDataFromFeatureDefIDs(int id, int attrID, sol::optional<int> teamIdOpt, sol::optional<int> elemOffsetOpt);
	size_t InstanceDataFromFeatureDefIDs(const sol::stack_table& ids, int attrID, sol::optional<int> teamIdOpt, sol::optional<int> elemOffsetOpt);
	size_t InstanceDataFromUnitIDs(int id, int attrID, sol::optional<int> elemOffsetOpt);
	size_t InstanceDataFromUnitIDs(const sol::stack_table& ids, int attrID, sol::optional<int> elemOffsetOpt);
	size_t InstanceDataFromFeatureIDs(int id, int attrID, sol::optional<int> elemOffsetOpt);
	size_t InstanceDataFromFeatureIDs(const sol::stack_table& ids, int attrID, sol::optional<int> elemOffsetOpt);

	size_t MatrixDataFromProjectileIDs(int id, int attrID, sol::optional<int> elemOffsetOpt);
	size_t MatrixDataFromProjectileIDs(const sol::stack_table& ids, int attrID, sol::optional<int> elemOffsetOpt);

	int BindBufferRange  (const GLuint index, const sol::optional<int> elemOffsetOpt, const sol::optional<int> elemCountOpt, const sol::optional<GLenum> targetOpt);
	int UnbindBufferRange(const GLuint index, const sol::optional<int> elemOffsetOpt, const sol::optional<int> elemCountOpt, const sol::optional<GLenum> targetOpt);

	void DumpDefinition();
public:
	static bool Supported(GLenum target);

	inline XBO* GetXBO() const { return xbo; }
	inline size_t GetAttributeCount() const { return (size_t)attributesCount; }

private:
	// performed by VAO, leaves information; information is used to generate intuitive `defaults`, such as vao:Draw(`count`)
	inline void MemorizedUpload(size_t destStartPos, size_t destEndPos, const void* data)
	{
		assert(destStartPos <= destEndPos);

		XBO* const xbo = GetXBO();
		xbo->Bind();
		xbo->SetBufferSubData(destStartPos, destEndPos-destStartPos, data);
		xbo->Unbind();

		lastMemorizedUploadEndPosition = destEndPos;
	}
	inline size_t GetLastMemorizedUploadEndPosition() const { return lastMemorizedUploadEndPosition; }

private:
	void AllocGLBuffer(size_t byteSize);
	void CopyAttrMapToVec();

	int BindBufferRangeImpl(GLuint index, const sol::optional<int> elemOffsetOpt, const sol::optional<int> elemCountOpt, const sol::optional<GLenum> targetOpt, bool bind);

	bool IsTypeValid(GLenum type);

	void GetTypePtr(GLenum type, GLint size, uint32_t& thisPointer, uint32_t& nextPointer, GLsizei& alignment, GLsizei& sizeInBytes);

	bool FillAttribsTableImpl(const sol::table& attrDefTable);
	bool FillAttribsNumberImpl(const int numVec4Attribs);
	bool DefineElementArray(const sol::optional<sol::object> attribDefArgOpt);
private:
	uint32_t GetId() const { return xbo->GetIdRaw(); }

	void UpdateModelsXBOElementCount();
	size_t ModelsXBOImpl();

	inline void InstanceBufferCheck(int attrID, const char* func);
	inline void InstanceBufferCheckAndFormatCheck(int attrID, const char* func);

	template<typename TObj>
	static SInstanceData InstanceDataFromGetData(int id, int attrID, uint8_t defTeamID);

	template<typename TObj>
	size_t InstanceDataFromImpl(int id, int attrID, uint8_t defTeamID, const sol::optional<int>& elemOffsetOpt);

	template<typename TObj>
	size_t InstanceDataFromImpl(const sol::stack_table& ids, int attrID, uint8_t defTeamID, const sol::optional<int>& elemOffsetOpt);

	template<typename Iterable>
	size_t MatrixDataFromProjectileIDsImpl(const Iterable& ids, int attrID, sol::optional<int> elemOffsetOpt, const char* func);

	template<typename TIn, typename AttribTestFunc>
	size_t UploadImpl(const std::vector<TIn>& dataVec, uint32_t elemOffset, AttribTestFunc attribTestFunc);

	template<typename TIn>
	size_t UploadImpl(const std::vector<TIn>& dataVec, uint32_t elemOffset, int attribIdx) {
		return UploadImpl(dataVec, elemOffset, [attribIdx](int attrID) {
			return attribIdx == -1 || attribIdx == attrID; // copy data if specific attribIdx is not requested or requested and matches attrID
		});
	}

	template<typename TIn>
	size_t UploadImpl(const std::vector<TIn>& dataVec, uint32_t elemOffset, const std::initializer_list<int>& attribIdxs) {
		return UploadImpl(dataVec, elemOffset, [&attribIdxs](int attrID) {
			return std::find(attribIdxs.begin(), attribIdxs.end(), attrID) != attribIdxs.end();
		});
	}

	template<typename T>
	static T MaybeFunc(const sol::table& tbl, const std::string& key, T defValue);

	template<typename TIn, typename TOut, typename TIter>
	bool TransformAndWrite(int& bytesWritten, GLubyte*& mappedBuf, const int mappedBufferSizeInBytes, const int size, TIter& bdvIter, const TIter& bdvIterEnd, const bool copyData);

	template<typename TIn>
	bool TransformAndRead(int& bytesRead, GLubyte*& mappedBuf, const int mappedBufferSizeInBytes, const int size, std::vector<lua_Number>& vec, const bool copyData);
private:
	friend class LuaVAOImpl;
private:
	struct BufferAttribDef {
		GLenum type;
		GLint size; // in number of elements of type
		GLboolean normalized; //VAO only
		std::string name;
		//AUX
		GLsizei pointer;
		GLsizei typeSizeInBytes;
		GLsizei strideSizeInBytes;
	};
private:
	GLenum defTarget;
	GLenum usageHint;

	uint32_t attributesCount;

	uint32_t elementsCount;
	uint32_t elemSizeInBytes;
	uint32_t bufferSizeInBytes;

	XBO* xbo = nullptr;
	bool xboOwner;

	void* bufferData;

	uint32_t primitiveRestartIndex;

	std::vector<std::pair<const int, const BufferAttribDef>> bufferAttribDefsVec;
	std::map<const int, BufferAttribDef> bufferAttribDefs;

	size_t lastMemorizedUploadEndPosition = 0;

private:
	static constexpr uint32_t uboMinIndex = 5 + 1; // glBindBufferBase(GL_UNIFORM_BUFFER, 5, uboGroundLighting.GetId()); //DecalsDrawerGL4
	static constexpr uint32_t ssboMinIndex = 3 + 1; // glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, uboDecalsStructures.GetId()); //DecalsDrawerGL4
private:
	static constexpr uint32_t VA_NUMBER_OF_ATTRIBUTES = 16u;
	static constexpr uint32_t UBO_SAFE_SIZE_BYTES = 0x4000u; //16 KB
	static constexpr uint32_t BUFFER_SANE_LIMIT_BYTES = 0x1000000u; //16 MB
	static constexpr GLenum DEFAULT_VERT_ATTR_TYPE = GL_FLOAT;
	static constexpr GLenum DEFAULT_BUFF_ATTR_TYPE = GL_FLOAT_VEC4;
	static constexpr GLenum DEFAULT_INDX_ATTR_TYPE = GL_UNSIGNED_SHORT;
};
