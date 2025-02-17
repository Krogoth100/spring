/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef _LOS_TEXTURE_H
#define _LOS_TEXTURE_H

#include "PboInfoTexture.h"
#include "Rendering/GL/FBO.h"


namespace Shader {
	struct IProgramObject;
}


class CLosTexture : public CPboInfoTexture
{
public:
	CLosTexture();

public:
	void Update() override;
	bool IsUpdateNeeded() override { return true; }
};

#endif // _LOS_TEXTURE_H
