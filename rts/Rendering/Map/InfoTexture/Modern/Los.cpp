/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "Los.h"
#include "Game/GlobalUnsynced.h"
#include "Rendering/GlobalRendering.h"
#include "Sim/Misc/LosHandler.h"
#include "System/Log/ILog.h"



CLosTexture::CLosTexture()
: CPboInfoTexture("los")
{
	texSize = losHandler->los.size;
	texChannels = 1;

	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glSpringTexStorage2D(GL_TEXTURE_2D, 1, GL_R8, texSize.x, texSize.y);

	infoTexPBO.Bind();
	infoTexPBO.New(texSize.x * texSize.y * texChannels * 2, GL_STREAM_DRAW);
	infoTexPBO.Unbind();
}


void CLosTexture::Update()
{
	infoTexPBO.Bind();
	auto infoTexMem = reinterpret_cast<unsigned char*>(infoTexPBO.MapBuffer());

	if (!losHandler->GetGlobalLOS(gu->myAllyTeam)) {
		const unsigned short* myLos = &losHandler->los.losMaps[gu->myAllyTeam].front();
		for (int y = 0; y < texSize.y; ++y) {
			for (int x = 0; x < texSize.x; ++x) {
				infoTexMem[y * texSize.x + x] = (myLos[y * texSize.x + x] != 0) ? 255 : 0;
			}
		}
	} else {
		memset(infoTexMem, 255, texSize.x * texSize.y);
	}

	infoTexPBO.UnmapBuffer();
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texSize.x, texSize.y, GL_RED, GL_UNSIGNED_BYTE, infoTexPBO.GetPtr());
	infoTexPBO.Invalidate();
	infoTexPBO.Unbind();
}
