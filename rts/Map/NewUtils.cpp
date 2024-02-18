#include "NewUtils.h"
#include "Map/ReadMap.h"
#include "Map/MapDamage.h"
#include "System/Log/ILog.h"
#include <string>

namespace {
	std::string HeightMapFilePath;
	float HeightBase, HeightScale;

	bool LoadHeightBitmap(CBitmap& bitmap, const char* filePath) {
		if (!bitmap.LoadGrayscale(std::string(filePath), true)) {
			LOG_L(L_ERROR, "[%s()]: Couldn't load \"%s\" bitmap!", __func__, filePath);
			return false;
		}
		//!clean - mapxp1/mapyp1 may be apparently not available
		//if (bitmap.xsize != mapDims.mapxp1 || bitmap.ysize != mapDims.mapyp1) {
		if (bitmap.xsize != mapDims.mapx+1 || bitmap.ysize != mapDims.mapy+1) {
			LOG_L(L_ERROR, "[%s()]: Incorrect \"%s\" bitmap dimensions!", __func__, filePath);
			return false;
		}
		return true;
	}
}

void SetHeightMapRequisites(const char* heightMapFilePath, float base, float scale) {
	HeightMapFilePath = heightMapFilePath;
	HeightBase = base;
	HeightScale = scale;
}

// A bitmap must be 16-bit grayscale
void SetHeightMapByBitmap(const CBitmap& bitmap, float heightBase, float heightScale, int fromX, int fromZ, int toX, int toZ) {
	if (mapDamage->Disabled()) return;

	HeightBase = heightBase;
	HeightScale = heightScale;

	const uint16_t* data = reinterpret_cast<const uint16_t*>(bitmap.GetRawMem());
	bool heightMapChanged = false;
	for (int z = fromZ; z <= toZ; ++z) {
		for (int x = fromX; x <= toX; ++x) {
			const int index = z*mapDims.mapxp1 +x;
			const float height = heightBase +float(data[index])/65535.0f*heightScale;
			const float oldHeight = readMap->GetCornerHeightMapSynced()[index];
			readMap->SetHeight(index, height);
			heightMapChanged = heightMapChanged || (height != oldHeight);
		}
	}

	if (heightMapChanged) {
		mapDamage->RecalcArea(fromX, toX, fromZ, toZ);
	}
}

void SetHeightMapByFile(const char* filePath, float heightBase, float heightScale, int fromX, int fromZ, int toX, int toZ) {
	if (mapDamage->Disabled()) return;

	CBitmap bitmap;
	if (!LoadHeightBitmap(bitmap, filePath)) return;

	HeightMapFilePath = filePath;
	SetHeightMapByBitmap(bitmap, heightBase, heightScale, fromX,fromZ, toX,toZ);
}

void GetHeightDataFromCurFile(float* destHeightData) {
	CBitmap bitmap;
	if (!LoadHeightBitmap(bitmap, HeightMapFilePath.c_str())) return;

	const uint16_t* bitmapData = reinterpret_cast<const uint16_t*>(bitmap.GetRawMem());
	float* const destHeightDataEnd = destHeightData +(mapDims.mapx+1)*(mapDims.mapy+1);
	for (; destHeightData != destHeightDataEnd; ++destHeightData, ++bitmapData) {
		*destHeightData = HeightBase +float(*bitmapData)/65535.0f*HeightScale;
	}
}