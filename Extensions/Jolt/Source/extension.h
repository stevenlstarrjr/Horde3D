// Horde3D Jolt Physics Extension
// SPDX-License-Identifier: EPL-1.0

#pragma once

#include "egExtensions.h"

namespace Horde3DJolt {

class ExtJolt : public Horde3D::IExtension
{
public:
	const char *getName() const override { return "JoltPhysics"; }
	bool init() override;
	void release() override;
};

}
