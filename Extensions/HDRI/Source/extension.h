// Horde3D HDRI / image-based lighting extension
// SPDX-License-Identifier: EPL-1.0

#pragma once

#include "egExtensions.h"

namespace Horde3DHDRI {

class ExtHDRI : public Horde3D::IExtension
{
public:
	const char *getName() const override { return "HDRI"; }
	bool init() override { return true; }
	void release() override {}
};

}
