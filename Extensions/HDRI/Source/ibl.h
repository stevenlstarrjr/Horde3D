// Horde3D HDRI / image-based lighting extension
// SPDX-License-Identifier: EPL-1.0

#pragma once

#include "Horde3DHDRI.h"

namespace Horde3DHDRI {

bool createIbl(H3DRes equirectangularTexture, const char *resourcePrefix,
	const H3DHdriSettings *settings, H3DHdriIbl *outIbl);
bool bindIbl(H3DRes material, const H3DHdriIbl *ibl);
void releaseIbl(H3DHdriIbl *ibl);

}
