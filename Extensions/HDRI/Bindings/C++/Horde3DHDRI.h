// Horde3D HDRI / image-based lighting extension
// SPDX-License-Identifier: EPL-1.0

#pragma once

#include "Horde3D.h"

enum { H3D_HDRI_SPECULAR_LEVELS = 5 };

struct H3DHdriSettings
{
	int environmentSize = 128;
	int irradianceSize = 32;
	int specularSize = 64;
	int brdfLutSize = 128;
	int sampleCount = 128;
	float lightingSaturation = 1.0f;
	// Scales the irradiance and prefiltered specular maps. The source environment
	// remains unscaled so applications can choose their own display exposure.
	float lightingIntensity = 1.0f;
};

struct H3DHdriIbl
{
	H3DRes environment = 0;
	H3DRes irradiance = 0;
	H3DRes specular[H3D_HDRI_SPECULAR_LEVELS] = { 0, 0, 0, 0, 0 };
	H3DRes brdfLut = 0;
};

// Converts a loaded, equirectangular floating-point texture into runtime IBL resources.
// Radiance .hdr files loaded by Horde3D are accepted directly. Generated resource names are
// <prefix>.environment, <prefix>.irradiance, <prefix>.specular0..4, and <prefix>.brdfLut.
H3D_API bool h3dHdriCreateIbl(H3DRes equirectangularTexture, const char *resourcePrefix,
	const H3DHdriSettings *settings, H3DHdriIbl *outIbl);

// Binds generated resources to matching sampler names on a material:
// iblIrradiance, iblSpecular0..4, and iblBrdfLut.
H3D_API bool h3dHdriBindIbl(H3DRes material, const H3DHdriIbl *ibl);

// Removes every generated resource contained in the set and clears its handles.
H3D_API void h3dHdriReleaseIbl(H3DHdriIbl *ibl);
