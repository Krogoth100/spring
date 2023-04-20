/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef QTPFS_PATHMANAGER_HDR
#define QTPFS_PATHMANAGER_HDR

#include <vector>

#include "Sim/Path/IPathManager.h"
#include "NodeLayer.h"
#include "PathCache.h"
#include "PathSearch.h"
#include "System/UnorderedMap.hpp"

struct MoveDef;
struct SRectangle;
class CSolidObject;


namespace QTPFS {
	struct QTNode;
	class PathManager: public IPathManager {
	public:
		static constexpr unsigned int DAMAGE_MAP_BLOCK_SIZE = 16;
		struct MapChangeTrack {
			std::vector<bool> damageMap;
			std::deque<int> damageQueue;
			int width = 0;
			int height = 0;
		};

		PathManager();
		~PathManager();

		static void InitStatic();

		std::int32_t GetPathFinderType() const override { return QTPFS_TYPE; }
		std::uint32_t GetPathCheckSum() const override { return pfsCheckSum; }

		std::int64_t Finalize() override;

		bool PathUpdated(unsigned int pathID) override;

		void TerrainChange(unsigned int x1, unsigned int z1,  unsigned int x2, unsigned int z2, unsigned int type) override;
		void Update() override;
		void UpdatePath(const CSolidObject* owner, unsigned int pathID) override;
		void DeletePath(unsigned int pathID) override;

		unsigned int RequestPath(
			CSolidObject* object,
			const MoveDef* moveDef,
			float3 sourcePos,
			float3 targetPos,
			float radius,
			bool synced
		) override;

		float3 NextWayPoint(
			const CSolidObject*, // owner
			unsigned int pathID,
			unsigned int, // numRetries
			float3 point,
			float radius,
			bool synced
		) override;

		void GetPathWayPoints(
			unsigned int pathID,
			std::vector<float3>& points,
			std::vector<int>& starts
		) const override;

		int2 GetNumQueuedUpdates() const override;


		const NodeLayer& GetNodeLayer(unsigned int pathType) const { return nodeLayers[pathType]; }
		// const QTNode* GetNodeTree(unsigned int pathType) const { return nodeTrees[pathType]; }
		const PathCache& GetPathCache(unsigned int pathType) const { return pathCache; }

		const MapChangeTrack& GetMapChangeTrack() const { return mapChangeTrack; };

		// const spring::unordered_map<unsigned int, unsigned int>& GetPathTypes() const { return pathTypes; }
		const spring::unordered_map<unsigned int, PathSearchTrace::Execution*>& GetPathTraces() const { return pathTraces; }

	private:
		void MapChanged(int x1, int z1, int x2, int z2);

		void ThreadUpdate();
		void Load();

		std::uint64_t GetMemFootPrint() const;

		typedef void (PathManager::*MemberFunc)(
			unsigned int threadNum,
			unsigned int numThreads,
			const SRectangle& rect
		);
		typedef spring::unordered_map<unsigned int, unsigned int> PathTypeMap;
		typedef spring::unordered_map<unsigned int, unsigned int>::iterator PathTypeMapIt;
		typedef spring::unordered_map<unsigned int, PathSearchTrace::Execution*> PathTraceMap;
		typedef spring::unordered_map<unsigned int, PathSearchTrace::Execution*>::iterator PathTraceMapIt;
		typedef spring::unordered_map<std::uint64_t, IPath*> SharedPathMap;
		typedef spring::unordered_map<std::uint64_t, IPath*>::iterator SharedPathMapIt;

		typedef std::vector<PathSearch*> PathSearchVect;
		typedef std::vector<PathSearch*>::iterator PathSearchVectIt;

		void InitNodeLayersThreaded(const SRectangle& rect);
		void InitNodeLayer(unsigned int layerNum, const SRectangle& r);
		void UpdateNodeLayer(unsigned int layerNum, const SRectangle& r, int currentThread);

		void ExecuteQueuedSearches();
		void QueueDeadPathSearches();

		unsigned int QueueSearch(
			const CSolidObject* object,
			const MoveDef* moveDef,
			const float3& sourcePoint,
			const float3& targetPoint,
			const float radius,
			const bool synced
		);

		unsigned int RequeueSearch(
			IPath* oldPath
		);

		bool ExecuteSearch(
			// PathSearchVect& searches,
			// PathSearchVectIt& searchesIt,
			PathSearch* search,
			NodeLayer& nodeLayer,
			PathCache& pathCache,
			unsigned int pathType
		);

		bool IsFinalized() const { return isFinalized; }
			// return (!nodeTrees.empty()); }


		std::string GetCacheDirName(const std::string& mapCheckSumHexStr, const std::string& modCheckSumHexStr) const;
		void Serialize(const std::string& cacheFileDir);

	public:
		static std::vector<NodeLayer> nodeLayers;

	private:
		PathCache pathCache;
		// static std::vector<PathSearch*> pathSearches;

		// per thread data
		std::vector<SearchThreadData> searchThreadData;
		std::vector<UpdateThreadData> updateThreadData;

		// spring::unordered_map<unsigned int, unsigned int> pathTypes;
		spring::unordered_map<unsigned int, PathSearchTrace::Execution*> pathTraces;

		// maps "hashes" of executed searches to the found paths
		spring::unordered_map<std::uint64_t, IPath*> sharedPaths;

		// std::vector<unsigned int> numCurrExecutedSearches;
		// std::vector<unsigned int> numPrevExecutedSearches;

		MapChangeTrack mapChangeTrack;

		int deadPathsToUpdatePerFrame = 1;
		int recalcDeadPathUpdateRateOnFrame = 0;

		static unsigned int LAYERS_PER_UPDATE;
		static unsigned int MAX_TEAM_SEARCHES;

		unsigned int searchStateOffset;
		// unsigned int numTerrainChanges;
		unsigned int numPathRequests;
		unsigned int maxNumLeafNodes;

		std::int32_t refreshDirtyPathRateFrame = QTPFS_LAST_FRAME;
		std::int32_t updateDirtyPathRate = 0;
		std::int32_t updateDirtyPathRemainder = 0;

		std::uint32_t pfsCheckSum;

		entt::entity systemEntity = entt::null;

		bool layersInited;
		bool haveCacheDir;
		bool isFinalized = false;
	};
}

#endif

