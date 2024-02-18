/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */

#pragma once

#include "Rendering/Textures/Bitmap.h"
#include "Map/ReadMap.h"

void SetHeightMapRequisites(const char* heightMapFilePath, float base, float scale);

void SetHeightMapByBitmap(const CBitmap& bitmap, float heightBase, float heightScale, int fromX, int fromZ, int toX, int toZ);
inline void SetHeightMapByBitmap(const CBitmap& bitmap, float heightBase, float heightScale)
	{ SetHeightMapByBitmap(bitmap, heightBase, heightScale, 0,0, mapDims.mapx,mapDims.mapy); };

void SetHeightMapByFile(const char* filePath, float heightBase, float heightScale, int fromX, int fromZ, int toX, int toZ);
inline void SetHeightMapByFile(const char* filePath, float heightBase, float heightScale)
	{ SetHeightMapByFile(filePath, heightBase, heightScale, 0,0, mapDims.mapx,mapDims.mapy); };

void GetHeightDataFromCurFile(float* destHeightData);