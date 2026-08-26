// Horde3D HDRI / image-based lighting extension
// SPDX-License-Identifier: EPL-1.0

#include "extension.h"
#include "ibl.h"
#include "utPlatform.h"

namespace Horde3DHDRI {

H3D_IMPL bool h3dHdriCreateIbl(H3DRes equirectangularTexture, const char *resourcePrefix,
	const H3DHdriSettings *settings, H3DHdriIbl *outIbl)
{
	return createIbl(equirectangularTexture, resourcePrefix, settings, outIbl);
}

H3D_IMPL bool h3dHdriBindIbl(H3DRes material, const H3DHdriIbl *ibl)
{
	return bindIbl(material, ibl);
}

H3D_IMPL void h3dHdriReleaseIbl(H3DHdriIbl *ibl)
{
	releaseIbl(ibl);
}

}
