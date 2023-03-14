/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef QTPFS_NODELAYER_HDR
#define QTPFS_NODELAYER_HDR

// #undef NDEBUG

#include <limits>
#include <vector>
#include <deque>
#include <cinttypes>

#include "System/Rectangle.h"
#include "Node.h"
#include "PathDefines.h"

#include "System/Log/ILog.h"

#include "Registry.h"

struct MoveDef;

namespace QTPFS {
	struct INode;

	struct NodeLayer {
	public:
		typedef unsigned char  SpeedModType;
		typedef unsigned char SpeedBinType;

		static void InitStatic();
		static size_t MaxSpeedModTypeValue() { return (std::numeric_limits<SpeedModType>::max()); }
		static size_t MaxSpeedBinTypeValue() { return (std::numeric_limits<SpeedBinType>::max()); }

		NodeLayer() = default;
		NodeLayer(NodeLayer&& nl) = default;

		NodeLayer& operator = (const NodeLayer& nl) = delete;
		NodeLayer& operator = (NodeLayer&& nl) = default;

		void Init(unsigned int layerNum);
		void Clear();

		bool Update(
			const SRectangle& r,
			const MoveDef* md,
			const std::vector<float>* luSpeedMods = nullptr,
			const std::vector<  int>* luBlockBits = nullptr
		);

		void ExecNodeNeighborCacheUpdate(unsigned int currFrameNum, unsigned int currMagicNum);
		void ExecNodeNeighborCacheUpdates(const SRectangle& ur, unsigned int currMagicNum);

		float GetNodeRatio() const { return (numLeafNodes / std::max(1.0f, float(xsize * zsize))); }
		// const INode* GetNode(unsigned int x, unsigned int z) const { return nodeGrid[z * xsize + x]; }
		    //   INode* GetNode(unsigned int x, unsigned int z)       { return nodeGrid[z * xsize + x]; }
		// const INode* GetNode(unsigned int i) const { return nodeGrid[i]; }
		//       INode* GetNode(unsigned int i)       { return nodeGrid[i]; }

		const INode* GetNode(unsigned int x, unsigned int z) const {
			const INode* curNode = &rootNode;
			while (!curNode->IsLeaf()) {
				bool isRight = x >= curNode->xmid(); // TODO: do these on CPU, don't reference memory !!!!!!!!!!!!!1
				bool isDown = z >= curNode->zmid();
				int offset = 1*(isRight) + 2*(isDown);
				int nextIndex = curNode->GetChildBaseIndex() + offset;
				curNode = GetPoolNode(nextIndex);
			}
			assert(curNode == nodeGrid[z * xsize + x]);
			return curNode;
		}
		INode* GetNode(unsigned int x, unsigned int z) {
			const auto* cthis = this;
			return const_cast<INode*>(cthis->GetNode(x, z));
		}

		const INode* GetPoolNode(unsigned int i) const { return &poolNodes[i / POOL_CHUNK_SIZE][i % POOL_CHUNK_SIZE]; }
		      INode* GetPoolNode(unsigned int i)       { return &poolNodes[i / POOL_CHUNK_SIZE][i % POOL_CHUNK_SIZE]; }

		INode* AllocRootNode(const INode* parent, unsigned int nn,  unsigned int x1, unsigned int z1, unsigned int x2, unsigned int z2) {
			rootNode.Init(parent, nn, x1, z1, x2, z2, -1);
			return &rootNode;
		}

		unsigned int AllocPoolNode(const INode* parent, unsigned int nn,  unsigned int x1, unsigned int z1, unsigned int x2, unsigned int z2) {
			unsigned int idx = -1u;

			if (nodeIndcs.empty())
				return idx;

			if (poolNodes[(idx = nodeIndcs.back()) / POOL_CHUNK_SIZE].empty())
				poolNodes[idx / POOL_CHUNK_SIZE].resize(POOL_CHUNK_SIZE);

			poolNodes[idx / POOL_CHUNK_SIZE][idx % POOL_CHUNK_SIZE].Init(parent, nn, x1, z1, x2, z2, idx);
			nodeIndcs.pop_back();

			// LOG("%s: [%p] alloc'ed id=%d", __func__, &poolNodes, idx);

			return idx;
		}

		void FreePoolNode(unsigned int nodeIndex) {
			//LOG("%s: [%p] free'ed id=%d", __func__, &poolNodes, nodeIndex);
			nodeIndcs.push_back(nodeIndex); }


		// const std::vector<SpeedBinType>& GetOldSpeedBins() const { return oldSpeedBins; }
		const std::vector<SpeedBinType>& GetCurSpeedBins() const { return curSpeedBins; }
		// const std::vector<SpeedModType>& GetOldSpeedMods() const { return oldSpeedMods; }
		const std::vector<SpeedModType>& GetCurSpeedMods() const { return curSpeedMods; }

		std::vector<INode*>& GetNodes() { return nodeGrid; }

		void RegisterNode(INode* n);

		void SetNumLeafNodes(unsigned int n) { numLeafNodes = n; }
		unsigned int GetNumLeafNodes() const { return numLeafNodes; }

		float GetMaxRelSpeedMod() const { return maxRelSpeedMod; }
		float GetAvgRelSpeedMod() const { return avgRelSpeedMod; }

		SpeedBinType GetSpeedModBin(float absSpeedMod, float relSpeedMod) const;

		std::uint64_t GetMemFootPrint() const {
			std::uint64_t memFootPrint = sizeof(NodeLayer);
			memFootPrint += (curSpeedMods.size() * sizeof(SpeedModType));
			// memFootPrint += (oldSpeedMods.size() * sizeof(SpeedModType));
			memFootPrint += (curSpeedBins.size() * sizeof(SpeedBinType));
			// memFootPrint += (oldSpeedBins.size() * sizeof(SpeedBinType));
			memFootPrint += (nodeGrid.size() * sizeof(decltype(nodeGrid)::value_type));
			for (size_t i = 0, n = NUM_POOL_CHUNKS; i < n; i++) {
				memFootPrint += (poolNodes[i].size() * sizeof(QTNode));

				for (size_t j = poolNodes[i].size(); j > 0; --j) {
					int32_t nodeIndex = j - 1;
					const auto& neighbours = poolNodes[i][nodeIndex].GetNeighbours();
					memFootPrint += neighbours.size() * sizeof(std::remove_reference_t<decltype(neighbours)>::value_type);
					const auto& netPoints = poolNodes[i][nodeIndex].GetNetPoints();
					memFootPrint += neighbours.size() * sizeof(std::remove_reference_t<decltype(netPoints)>::value_type);
				}
			}

			memFootPrint += (nodeIndcs.size() * sizeof(decltype(nodeIndcs)::value_type));
			return memFootPrint;
		}

	private:
		std::vector<INode*> nodeGrid;

		std::vector<QTNode> poolNodes[16];
		std::vector<unsigned int> nodeIndcs;

		std::vector<SpeedModType> curSpeedMods;
		// std::vector<SpeedModType> oldSpeedMods;
		std::vector<SpeedBinType> curSpeedBins;
		// std::vector<SpeedBinType> oldSpeedBins;

		// root lives outside pool s.t. all four children of a given node are always in one chunk
		QTNode rootNode;

public:
		static constexpr unsigned int NUM_POOL_CHUNKS = sizeof(poolNodes) / sizeof(poolNodes[0]);
		static constexpr unsigned int POOL_TOTAL_SIZE = (1024 * 1024) / 2;
		static constexpr unsigned int POOL_CHUNK_SIZE = POOL_TOTAL_SIZE / NUM_POOL_CHUNKS;

private:

		// NOTE:
		//   we need a fixed range that does not become wider / narrower
		//   during terrain deformations (otherwise the bins would change
		//   across ALL nodes)
		static unsigned int NUM_SPEEDMOD_BINS;
		static float        MIN_SPEEDMOD_VALUE;
		static float        MAX_SPEEDMOD_VALUE;

		unsigned int layerNumber = 0;
		unsigned int numLeafNodes = 0;
		unsigned int updateCounter = 0;

		unsigned int xsize = 0;
		unsigned int zsize = 0;

		float maxRelSpeedMod = 0.0f;
		float avgRelSpeedMod = 0.0f;
	};
}

#endif

