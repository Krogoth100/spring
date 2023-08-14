#include "LuaVAOImpl.h"

#include <algorithm>
#include <type_traits>

#include "lib/fmt/format.h"
#include "lib/fmt/printf.h"

#include "lib/sol2/sol.hpp"

#include "System/SafeUtil.h"
#include "Rendering/GL/XBO.h"
#include "Rendering/GL/VAO.h"
#include "Rendering/GL/glHelpers.h"
#include "LuaXBOImpl.h"
#include "Rendering/Models/3DModel.h"
#include "Sim/Units/UnitHandler.h"
#include "Sim/Features/FeatureHandler.h"
#include "System/Log/ILog.h" // temp

#include "LuaUtils.h"


/******************************************************************************
 * Vertex Array Object
 * @classmod VAO
 *
 * @see LuaVAO.GetVAO
 * @see rts/Lua/LuaVAOImpl.cpp
******************************************************************************/


LuaVAOImpl::LuaVAOImpl(const LuaXBOImplSP& luaVBO, const sol::optional<LuaXBOImplSP>& luaIBO, const sol::optional<LuaXBOImplSP>& luaSBO)
	: vao{nullptr}

	, luaVBO(luaVBO)
	, luaSBO(luaSBO.value_or(nullptr))
	, luaIBO(luaIBO.value_or(nullptr))

	, baseInstance{0u}
{
	/*if (luaVBO && luaSBO) {
		for (const auto& v : luaVBO->bufferAttribDefs) {
			for (const auto& i : luaSBO->bufferAttribDefs) {
				if (v.first == i.first) {
					LuaUtils::SolLuaError("[LuaVAOImpl::%s] Vertex and Instance LuaXBO have defined a duplicate attribute [%d]", __func__, v.first);
				}
			}
		}
	}*/
}


/***
 *
 * @function VAO:Delete
 * @treturn nil
 */
void LuaVAOImpl::Delete()
{
	luaVBO = nullptr;
	luaSBO = nullptr;
	luaIBO = nullptr;

	vao = nullptr;
}

LuaVAOImpl::~LuaVAOImpl()
{
	Delete();
}

bool LuaVAOImpl::Supported()
{
	static bool supported = XBO::IsSupported(GL_ARRAY_BUFFER) && VAO::IsSupported() && GLEW_ARB_instanced_arrays && GLEW_ARB_draw_elements_base_vertex && GLEW_ARB_multi_draw_indirect;
	return supported;
}


template<typename TObj>
const SIndexAndCount LuaVAOImpl::GetDrawIndicesImpl(int id)
{
	const TObj* obj = LuaUtils::SolIdToObject<TObj>(id, __func__); //wrong ids are handles in LuaUtils::SolIdToObject<>()
	return GetDrawIndicesImpl<TObj>(obj);
}

template<typename TObj>
const SIndexAndCount LuaVAOImpl::GetDrawIndicesImpl(const TObj* obj)
{
	static_assert(std::is_base_of_v<CSolidObject, TObj> || std::is_base_of_v<SolidObjectDef, TObj>);

	S3DModel* model = obj->model;
	assert(model);
	return SIndexAndCount(model->indxStart, model->indxCount);
}

template<typename TObj>
int LuaVAOImpl::AddObjectsToSubmissionImpl(int id)
{
	DrawCheckInput inputs = {
		std::nullopt,          // drawCount
		std::nullopt,	       // baseVertex
		std::nullopt,	       // baseIndex
		submitCmds.size() + 1, // instCount
		std::nullopt,          // baseInstance
	};

	DrawCheck(GL_TRIANGLES, inputs, true);
	submitCmds.emplace_back(DrawObjectGetCmdImpl<TObj>(id));

	return submitCmds.size() - 1;
}

template<typename TObj>
int LuaVAOImpl::AddObjectsToSubmissionImpl(const sol::stack_table& ids)
{
	const std::size_t idsSize = ids.size(); //size() is very costly to do in the loop

	DrawCheckInput inputs = {
		std::nullopt,                // drawCount
		std::nullopt,	             // baseVertex
		std::nullopt,	             // baseIndex
		submitCmds.size() + idsSize, // instCount
		std::nullopt,                // baseInstance
	};

	DrawCheck(GL_TRIANGLES, inputs, true);

	constexpr auto defaultValue = static_cast<lua_Number>(0);
	for (std::size_t i = 0u; i < idsSize; ++i) {
		lua_Number idLua = ids.raw_get_or<lua_Number>(i + 1, defaultValue);
		int id = spring::SafeCast<int, lua_Number>(idLua);

		submitCmds.emplace_back(DrawObjectGetCmdImpl<TObj>(id));
	}

	return submitCmds.size() - idsSize;
}

template<typename TObj>
SDrawElementsIndirectCommand LuaVAOImpl::DrawObjectGetCmdImpl(int id)
{
	const auto& indexAndCount = LuaVAOImpl::GetDrawIndicesImpl<TObj>(id);

	return std::move(SDrawElementsIndirectCommand{
		indexAndCount.count,
		1u,
		indexAndCount.index,
		0u,
		baseInstance++
	});
}

void LuaVAOImpl::CheckDrawPrimitiveType(GLenum mode) const
{
	switch (mode) {
	case GL_POINTS:
	case GL_LINE_STRIP:
	case GL_LINE_LOOP:
	case GL_LINES:
	case GL_LINE_STRIP_ADJACENCY:
	case GL_LINES_ADJACENCY:
	case GL_TRIANGLE_STRIP:
	case GL_TRIANGLE_FAN:
	case GL_TRIANGLES:
	case GL_TRIANGLE_STRIP_ADJACENCY:
	case GL_TRIANGLES_ADJACENCY:
	case GL_PATCHES:
		break;
	default:
		LuaUtils::SolLuaError("[LuaVAOImpl::%s]: Using deprecated or incorrect primitive type (%d)", __func__, mode);
	}
}

void LuaVAOImpl::EnsureVAOInit()
{
	if (vao &&
		(luaVBO && luaVBO->GetId() == oldVBOId) &&
		(luaIBO && luaIBO->GetId() == oldIBOId) &&
		(luaSBO && luaSBO->GetId() == oldSBOId))
		return; //already init and all XBOs still have same IDs

	vao = nullptr;
	vao = std::make_unique<VAO>();
	vao->Bind();

	if (luaVBO) {
		luaVBO->xbo->Bind(GL_ARRAY_BUFFER); //type is needed cause same buffer could have been rebounded as something else using LuaXBOs functions
		oldVBOId = luaVBO->GetId();
	}

	if (luaIBO) {
		luaIBO->xbo->Bind(GL_ELEMENT_ARRAY_BUFFER);
		oldIBOId = luaIBO->GetId();
	}

	#define INT2PTR(x) (reinterpret_cast<void*>(static_cast<intptr_t>(x)))

	GLenum indMin = ~0u;
	GLenum indMax =  0u;

	const auto glVertexAttribPointerFunc = [](GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* pointer) {
		if (type == GL_FLOAT || normalized)
			glVertexAttribPointer(index, size, type, normalized, stride, pointer);
		else //assume int types
			glVertexAttribIPointer(index, size, type, stride, pointer);
	};

	if (luaVBO)
		for (const auto& va : luaVBO->bufferAttribDefsVec) {
			const auto& attr = va.second;
			glEnableVertexAttribArray(va.first);
			glVertexAttribPointerFunc(va.first, attr.size, attr.type, attr.normalized, luaVBO->elemSizeInBytes, INT2PTR(attr.pointer));
			glVertexAttribDivisor(va.first, 0);
			indMin = std::min(indMin, static_cast<GLenum>(va.first));
			indMax = std::max(indMax, static_cast<GLenum>(va.first));
		}

	if (luaSBO) {
		if (luaVBO)
			luaVBO->xbo->Unbind();

		luaSBO->xbo->Bind(GL_ARRAY_BUFFER);
		oldSBOId = luaSBO->GetId();

		// start instance attribute indices right after vertex attributes; necessary for proxy dispatchers to maintain reasonable attribute layouts
		GLuint index = luaVBO->bufferAttribDefsVec.back().first +1;
		for (const auto& va : luaSBO->bufferAttribDefsVec) {
			const auto& attr = va.second;
			glEnableVertexAttribArray(index);
			glVertexAttribPointerFunc(index, attr.size, attr.type, attr.normalized, luaSBO->elemSizeInBytes, INT2PTR(attr.pointer));
			glVertexAttribDivisor(index, 1);
			indMin = std::min(indMin, static_cast<GLenum>(index));
			indMax = std::max(indMax, static_cast<GLenum>(index));
			++index;
		}
	}

	#undef INT2PTR

	vao->Unbind();

	if (luaVBO && luaVBO->xbo->bound)
		luaVBO->xbo->Unbind();

	if (luaSBO && luaSBO->xbo->bound)
		luaSBO->xbo->Unbind();

	if (luaIBO && luaIBO->xbo->bound)
		luaIBO->xbo->Unbind();

	//restore default state
	for (GLuint index = indMin; index <= indMax; ++index) {
		glVertexAttribDivisor(index, 0);
		glDisableVertexAttribArray(index);
	}
}

LuaVAOImpl::DrawCheckResult LuaVAOImpl::DrawCheck(GLenum mode, const DrawCheckInput& inputs, bool indexed)
{
	LuaVAOImpl::DrawCheckResult result{};

	if (luaVBO)
		luaVBO->UpdateModelsXBOElementCount(); //need to update elements count because underlyiing XBO could have been updated

	if (indexed) {
		if (!luaIBO)
			LuaUtils::SolLuaError("[LuaVAOImpl::%s]: No index buffer is attached. Did you succesfully call vao:AttachIndexBuffer()?", __func__);

		luaIBO->UpdateModelsXBOElementCount(); //need to update elements count because underlyiing XBO could have been updated

		result.baseIndex  = std::max(inputs.baseIndex.value_or(0) , 0);
		result.baseVertex = std::max(inputs.baseVertex.value_or(0), 0); //can't be checked easily

		result.drawCount = inputs.drawCount.value_or(luaIBO->elementsCount);
		if (!inputs.drawCount.has_value() || inputs.drawCount.value() <= 0)
			result.drawCount -= result.baseIndex; //adjust automatically

		if (result.drawCount <= 0)
			LuaUtils::SolLuaError("[LuaVAOImpl::%s]: Non-positive number of draw elements %d is requested", __func__, result.drawCount);

		if (result.drawCount > luaIBO->elementsCount - result.baseIndex)
			LuaUtils::SolLuaError("[LuaVAOImpl::%s]: Requested number of elements %d with offset %d exceeds buffer size %u", __func__, result.drawCount, result.baseIndex, luaIBO->elementsCount);

	} else {
		if (!luaVBO) {
			if (!inputs.drawCount.has_value())
				LuaUtils::SolLuaError("[LuaVAOImpl::%s]: In case vertex buffer is not attached, the drawCount param should be set explicitly", __func__);

			result.drawCount = inputs.drawCount.value();
		}
		else {
			result.drawCount = inputs.drawCount.value_or(luaVBO->elementsCount);

			if (!inputs.drawCount.has_value())
				result.drawCount -= result.baseIndex; //note baseIndex not baseVertex

			if (result.drawCount > luaVBO->elementsCount - result.baseIndex)
				LuaUtils::SolLuaError("[LuaVAOImpl::%s]: Requested number of vertices %d with offset %d exceeds buffer size %u", __func__, result.drawCount, result.baseIndex, luaVBO->elementsCount);
		}
	}

	result.baseInstance = std::max(inputs.baseInstance.value_or(0), 0);
	result.instCount = std::max(inputs.instCount.value_or(0), 0); // 0 - forces ordinary version, while 1 - calls *Instanced()

	if (result.instCount > 0) {
		if (luaSBO && (result.instCount > luaSBO->elementsCount - result.baseInstance))
			LuaUtils::SolLuaError("[LuaVAOImpl::%s]: Requested number of instances %d with offset %d exceeds buffer size %u", __func__, result.instCount, result.baseInstance, luaSBO->elementsCount);
	}
	else {
		if (result.baseInstance > 0)
			LuaUtils::SolLuaError("[LuaVAOImpl::%s]: Requested baseInstance [%d] and but zero instance count", __func__, result.baseInstance);
	}

	CheckDrawPrimitiveType(mode);
	EnsureVAOInit();

	return result;
}


void LuaVAOImpl::ClearSubmission()
{
	baseInstance = 0u;
	submitCmds.clear();
}


/***
 *
 * @function VAO:AddUnitDefsToSubmission
 * @tparam number|{number,...} unitDefIDs
 * @treturn number submittedCount
 */
int LuaVAOImpl::AddUnitDefsToSubmission(int id) { return AddObjectsToSubmissionImpl<UnitDef>(id); }
int LuaVAOImpl::AddUnitDefsToSubmission(const sol::stack_table& ids) { return AddObjectsToSubmissionImpl<UnitDef>(ids); }


/***
 *
 * @function VAO:AddFeatureDefsToSubmission
 * @tparam number|{number,...} featureDefIDs
 * @treturn number submittedCount
 */
int LuaVAOImpl::AddFeatureDefsToSubmission(int id) { return AddObjectsToSubmissionImpl<FeatureDef>(id); }
int LuaVAOImpl::AddFeatureDefsToSubmission(const sol::stack_table& ids) { return AddObjectsToSubmissionImpl<FeatureDef>(ids); }


/***
 *
 * @function VAO:RemoveFromSubmission
 * @tparam number index
 * @treturn nil
 */
void LuaVAOImpl::RemoveFromSubmission(int idx)
{
	if (idx < 0 || idx >= submitCmds.size()) {
		LuaUtils::SolLuaError("[LuaVAOImpl::%s] wrong submitCmds index [%d]", __func__, idx);
		return;
	}

	if (idx != submitCmds.size() - 1)
		submitCmds[idx] = submitCmds.back();

	submitCmds.pop_back();
	for (baseInstance = 0; baseInstance < submitCmds.size(); ++baseInstance) {
		submitCmds[baseInstance].baseInstance = baseInstance;
	}
}


/***
 *
 * @function VAO:Submit
 * @treturn nil
 */
void LuaVAOImpl::Submit()
{
	vao->Bind();
	glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, submitCmds.data(), submitCmds.size(), sizeof(SDrawElementsIndirectCommand));
	vao->Unbind();
}



///////////////////////////////////////////////////////////////////
//
//  Models


void LuaVAOImpl::EnsureBinsInit()
{
	assert(luaSBO->GetAttributeCount() == 1);

	if (!bins) bins = std::make_unique<Bins>(submitCmds);
}


template<typename TObj>
void LuaVAOImpl::Bins::UpdateImpl(const sol::stack_table& removedObjects, const sol::stack_table& addedObjects, sol::optional<size_t> removedCount, sol::optional<size_t> addedCount)
{
	const size_t removedObjectCount = (removedCount.has_value()? removedCount.value() : removedObjects.size());
	const size_t addedObjectCount = (addedCount.has_value()? addedCount.value() : addedObjects.size());
	if (removedObjectCount == 0 && addedObjectCount == 0) return;

	size_t firstChangedBinIndex = std::numeric_limits<size_t>::max();

	for (std::size_t i = 1; i <= removedObjectCount; ++i) {
		lua_Number objIdLua = removedObjects.raw_get<lua_Number>(i);
		const int objId = spring::SafeCast<int, lua_Number>(objIdLua);

		const TObj* obj = LuaUtils::SolIdToObject<TObj>(objId, __func__);
		const int modelId = obj->model->id;
		const size_t binIndex = modelIdToBinIndex[modelId];
		firstChangedBinIndex = std::min(firstChangedBinIndex, binIndex);
		auto& bin = bins[binIndex];

		#define UnorderedErase(vector, index) \
			(vector)[index] = (vector).back(); \
			(vector).pop_back()
		#define UnorderedMapSubstitute(map, substitutedKey, substitutionKey, val) \
			(map)[substitutionKey] = val; \
			(map).erase(substitutedKey)

		if (bin.objIds.size() == 1) {
			UnorderedMapSubstitute(modelIdToBinIndex, modelId, bins.back().modelId, binIndex);
			objIdToLocalInstance.erase(objId);
			UnorderedErase(bins, binIndex);
			UnorderedErase(submitCmds, binIndex);
			continue;
		}

		const size_t localInstance = objIdToLocalInstance[objId];
		UnorderedMapSubstitute(objIdToLocalInstance, objId, bin.objIds.back(), localInstance);
		UnorderedErase(bin.objIds, localInstance);
		UnorderedErase(bin.instanceData, localInstance);
		--submitCmds[binIndex].instanceCount;

		#undef UnorderedErase
		#undef UnorderedMapSubstitute
	}

	for (std::size_t i = 1; i <= addedObjectCount; ++i) {
		lua_Number objIdLua = addedObjects.raw_get<lua_Number>(i);
		const int objId = spring::SafeCast<int, lua_Number>(objIdLua);

		const TObj* obj = LuaUtils::SolIdToObject<TObj>(objId, __func__);
		const auto model = obj->model;
		const int modelId = model->id;

		auto binIndexIt = modelIdToBinIndex.find(modelId);
		size_t binIndex;

		if (binIndexIt == modelIdToBinIndex.end()) {
			binIndex = bins.size();
			modelIdToBinIndex[modelId] = binIndex;
			bins.emplace_back(modelId, obj->GetDef()->id);
			submitCmds.emplace_back(
				model->indxCount,
				0u,
				model->indxStart,
				0u,
				0u
			);
		} else {
			binIndex = binIndexIt->second;
		}

		firstChangedBinIndex = std::min(firstChangedBinIndex, binIndex);
		auto& bin = bins[binIndex];

		objIdToLocalInstance[objId] = bin.objIds.size();
		bin.objIds.emplace_back(objId);
		bin.instanceData.emplace_back(GetObjectInstanceData(obj));
		++submitCmds[binIndex].instanceCount;

		assert(bin.instanceData.back());
	}

	assert(bins.size() == submitCmds.size());

	firstChangedInstance = 0;
	for (size_t binIndex = 0; binIndex < firstChangedBinIndex; ++binIndex) {
		firstChangedInstance += bins[binIndex].objIds.size();
	}

	instanceData.resize(instanceData.size() +addedObjectCount -removedObjectCount);
	size_t instance = firstChangedInstance;
	for (size_t binIndex = firstChangedBinIndex; binIndex < bins.size(); ++binIndex) {
		const auto& localInstanceData = bins[binIndex].instanceData;

		assert(localInstanceData.size() == submitCmds[binIndex].instanceCount);

		std::copy(localInstanceData.begin(), localInstanceData.end(), instanceData.begin()+instance);
		submitCmds[binIndex].baseInstance = instance;
		instance += localInstanceData.size();
	}

	requireInstanceDataUpload = true;
}


/* Lua */
void LuaVAOImpl::UpdateUnitBins(const sol::stack_table& removedUnits, const sol::stack_table& addedUnits, sol::optional<size_t> removedCount, sol::optional<size_t> addedCount)
{
	EnsureBinsInit();
	bins->UpdateImpl<CUnit>(removedUnits, addedUnits, removedCount, addedCount);
}


/* Lua */
void LuaVAOImpl::UpdateFeatureBins(const sol::stack_table& removedFeatures, const sol::stack_table& addedFeatures, sol::optional<size_t> removedCount, sol::optional<size_t> addedCount)
{
	EnsureBinsInit();
	bins->UpdateImpl<CFeature>(removedFeatures, addedFeatures, removedCount, addedCount);
}


void LuaVAOImpl::UpdateBinsGPU()
{
	if (bins->requireInstanceDataUpload) {
		const size_t firstChangedInstance = bins->firstChangedInstance;
		luaSBO->MemorizedUpload(firstChangedInstance*sizeof(SInstanceData), bins->instanceData.size()*sizeof(SInstanceData), &bins->instanceData[firstChangedInstance]);

		bins->requireInstanceDataUpload = false;
	}
}


/* Lua */
void LuaVAOImpl::SubmitBins()
{
	EnsureBinsInit();
	EnsureVAOInit();

	UpdateBinsGPU();

	Submit();
}


/* Lua */
void LuaVAOImpl::SubmitBins(const sol::function binPrepFunc)
{
	EnsureBinsInit();
	EnsureVAOInit();

	UpdateBinsGPU();

	vao->Bind();

	auto submitCmdPtr = &submitCmds[0];
	for (const auto& bin : bins->bins) {
		binPrepFunc(bin.objIds.front(), bin.sampleDefId);
		glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, submitCmdPtr, 1, sizeof(SDrawElementsIndirectCommand));
		++submitCmdPtr;
	}

	vao->Unbind();
}


///////////////////////////////////////////////////////////////////
//
//  Custom shapes


/* Lua */
void LuaVAOImpl::SetDrawMode(GLenum drawMode_)
{
	drawMode = drawMode_;
}


/* Lua */
void LuaVAOImpl::Draw(sol::optional<GLsizei> count)
{
	EnsureVAOInit();

	vao->Bind();

	const GLsizei instanceCount = count.value_or(luaSBO? luaSBO->GetLastMemorizedUploadEndPosition()/sizeof(SInstanceData) : 1);
	if (luaIBO) {
		glDrawElementsInstanced(drawMode, luaIBO->elementsCount, LuaXBOImpl::DEFAULT_INDX_ATTR_TYPE, 0, instanceCount);
	} else {
		glDrawArraysInstanced(drawMode, 0, luaVBO->elementsCount, instanceCount);
	}

	vao->Unbind();
}


/* Lua */
void LuaVAOImpl::DrawReusedBins(const LuaVAOImplSP& luaVAO, const sol::function binGateFunc)
{
	assert(luaVAO->bins);

	EnsureVAOInit();

	vao->Bind();

	const auto& bins = luaVAO->bins->bins;

	GLuint binFirstInstance = 0;
	if (luaIBO) {
		for (const auto& bin : bins) {
			sol::optional<bool> passed = binGateFunc(bin.objIds.front(), bin.sampleDefId);
			if (passed.value_or(true))
				glDrawElementsInstancedBaseInstance(drawMode, luaIBO->elementsCount, LuaXBOImpl::DEFAULT_INDX_ATTR_TYPE, 0, bin.instanceData.size(), binFirstInstance);
			binFirstInstance += bin.instanceData.size();
		}
	} else {
		for (const auto& bin : bins) {
			sol::optional<bool> passed = binGateFunc(bin.objIds.front(), bin.sampleDefId);
			if (passed.value_or(true))
				glDrawArraysInstancedBaseInstance(drawMode, 0, luaVBO->elementsCount, bin.instanceData.size(), binFirstInstance);
			binFirstInstance += bin.instanceData.size();
		}
	}

	vao->Unbind();
}
