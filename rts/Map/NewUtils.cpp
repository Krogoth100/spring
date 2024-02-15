#include "NewUtils.h"
#include "Map/ReadMap.h"
#include "Map/MapDamage.h"
#include "System/Log/ILog.h"

// A bitmap must be 16-bit grayscale
void SetHeightMapByBitmap(const CBitmap& bitmap, float heightBase, float heightScale, int fromX, int fromZ, int toX, int toZ) {
	if (mapDamage->Disabled()) return;

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

	if (!bitmap.LoadGrayscale(std::string(filePath), true)) {
		LOG_L(L_ERROR, "[%s()]: Couldn't load \"%s\" bitmap!", __func__, filePath);
		return;
	}

	if (bitmap.xsize != mapDims.mapxp1 || bitmap.ysize != mapDims.mapyp1) {
		LOG_L(L_ERROR, "[%s()]: Incorrect \"%s\" bitmap dimensions!", __func__, filePath);
		return;
	}

	SetHeightMapByBitmap(bitmap, heightBase, heightScale, fromX,fromZ, toX,toZ);
}