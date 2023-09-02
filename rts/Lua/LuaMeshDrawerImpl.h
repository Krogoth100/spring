/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#pragma once

#include "lib/sol2/forward.hpp"
#include "Rendering/GL/myGL.h"
#include "Rendering/Models/3DModelVAO.h"
#include "System/UnorderedMap.hpp"

#include <map>
#include <string>
#include <memory>
#include <vector>

class VAO;
class LuaXBOImpl;

class LuaMeshDrawerImpl {
public:
	class Bins;

	using LuaXBOImplSP = std::shared_ptr<LuaXBOImpl>; //my workaround to https://github.com/ThePhD/sol2/issues/1206
	using LuaMeshDrawerImplSP = std::shared_ptr<LuaMeshDrawerImpl>;
public:
	LuaMeshDrawerImpl(const LuaXBOImplSP& luaVBO, const sol::optional<LuaXBOImplSP>& luaIBO, const sol::optional<LuaXBOImplSP>& luaSBO);

	LuaMeshDrawerImpl(const LuaMeshDrawerImpl&) = delete;
	LuaMeshDrawerImpl(LuaMeshDrawerImpl&&) = default;

	void Delete();
	~LuaMeshDrawerImpl();
public:
	static bool Supported();
public:
// Models
	void UpdateUnitBins(const sol::stack_table& removedUnits, const sol::stack_table& addedUnits, sol::optional<size_t> removedCount, sol::optional<size_t> addedCount);
	void UpdateFeatureBins(const sol::stack_table& removedFeatures, const sol::stack_table& addedFeatures, sol::optional<size_t> removedCount, sol::optional<size_t> addedCount);
	void SubmitBins();
	void SubmitBins(const sol::function binPrepFunc);

// Custom shapes
	void SetDrawMode(GLenum drawMode);
	void Draw(sol::optional<GLsizei> count);
	void DrawReusedBins(const LuaMeshDrawerImplSP& luaMeshDrawer, const sol::function binGateFunc);

	// todo: cleanup
	void ClearSubmission();
	int AddUnitDefsToSubmission(int id);
	int AddUnitDefsToSubmission(const sol::stack_table& ids);
	int AddFeatureDefsToSubmission(int id);
	int AddFeatureDefsToSubmission(const sol::stack_table& ids);
	void RemoveFromSubmission(int idx);
	void Submit();
private:
	template<typename T>
	struct DrawCheckType {
		DrawCheckType() = default;
		DrawCheckType(T drawCount_, T baseVertex_, T baseIndex_, T instCount_, T baseInstance_)
			: drawCount{ std::move(drawCount_) }
			, baseVertex{ std::move(baseVertex_) }
			, baseIndex{ std::move(baseIndex_) }
			, instCount{ std::move(instCount_) }
			, baseInstance{ std::move(baseInstance_) }
		{};
		T drawCount;
		T baseVertex;
		T baseIndex;
		T instCount;
		T baseInstance;
	};
	using DrawCheckInput  = DrawCheckType<sol::optional<int>>;
	using DrawCheckResult = DrawCheckType<int>;

	[[maybe_unused]] DrawCheckResult DrawCheck(GLenum mode, const DrawCheckInput& inputs, bool indexed);
	void EnsureVAOInit();
	void CheckDrawPrimitiveType(GLenum mode) const;
	void AttachBufferImpl(const std::shared_ptr<LuaXBOImpl>& luaXBO, std::shared_ptr<LuaXBOImpl>& thisLuaXBO, GLenum reqTarget);

	void EnsureBinsInit();
private:
	template <typename TObj>
	int AddObjectsToSubmissionImpl(int id);
	template <typename TObj>
	int AddObjectsToSubmissionImpl(const sol::stack_table& ids);
	template <typename TObj>
	SDrawElementsIndirectCommand DrawObjectGetCmdImpl(int id);
	template <typename TObj>
	static const SIndexAndCount GetDrawIndicesImpl(int id);
	template <typename TObj>
	static const SIndexAndCount GetDrawIndicesImpl(const TObj* obj);

	void UpdateBinsGPU();
private:
	std::unique_ptr<VAO> vao = nullptr;

	std::shared_ptr<LuaXBOImpl> luaVBO;
	std::shared_ptr<LuaXBOImpl> luaIBO;
	std::shared_ptr<LuaXBOImpl> luaSBO;

	uint32_t oldVBOId = 0;
	uint32_t oldIBOId = 0;
	uint32_t oldSBOId = 0;

	std::unique_ptr<Bins> bins = nullptr;

	GLenum drawMode = GL_TRIANGLES;
	uint32_t baseInstance;
	std::vector<SDrawElementsIndirectCommand> submitCmds;
};

class LuaMeshDrawerImpl::Bins {
public:
	inline Bins(std::vector<SDrawElementsIndirectCommand>& submitCmds_)
	:	submitCmds(submitCmds_) {}

	struct Bin {
		inline Bin(int modelId, int sampleDefId) : modelId(modelId), sampleDefId(sampleDefId) {}

		int modelId, sampleDefId;
		std::vector<int> objIds;
		std::vector<SInstanceData> instanceData;
	};
	std::vector<Bin> bins;

	spring::unordered_map<int, size_t> modelIdToBinIndex;
	spring::unordered_map<int, size_t> objIdToLocalInstance;

	std::vector<SInstanceData> instanceData;
	bool requireInstanceDataUpload = false;
	size_t firstChangedInstance;
	std::vector<SDrawElementsIndirectCommand>& submitCmds;

	template <typename TObj>
	void UpdateImpl(const sol::stack_table& removedObjects, const sol::stack_table& addedObjects, sol::optional<size_t> removedCount, sol::optional<size_t> addedCount);
};
