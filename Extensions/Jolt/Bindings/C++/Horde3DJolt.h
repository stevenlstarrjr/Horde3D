// Horde3D Jolt Physics Extension
// SPDX-License-Identifier: EPL-1.0

#pragma once

#include "Horde3D.h"

typedef int H3DJoltBody;

struct H3DJoltMotionType
{
	enum List
	{
		Static = 0,
		Kinematic = 1,
		Dynamic = 2
	};
};

// Changes gravity for the physics world. Horde3D and Jolt both use Y as the up axis.
H3D_API void h3dJoltSetGravity(float x, float y, float z);

// Creates a rigid body at the node's current world transform. Dimensions are in world units.
// Box dimensions are half extents. Capsule halfHeight excludes its hemispherical caps.
// A positive mass is required for dynamic bodies and ignored for other motion types.
H3D_API H3DJoltBody h3dJoltAddBoxBody(H3DNode node, float halfX, float halfY, float halfZ,
	int motionType, float mass);
H3D_API H3DJoltBody h3dJoltAddSphereBody(H3DNode node, float radius,
	int motionType, float mass);
H3D_API H3DJoltBody h3dJoltAddCapsuleBody(H3DNode node, float halfHeight, float radius,
	int motionType, float mass);

H3D_API bool h3dJoltRemoveBody(H3DJoltBody body);
H3D_API H3DJoltBody h3dJoltGetNodeBody(H3DNode node);

// Advances physics and synchronizes transforms. Kinematic nodes drive their bodies before the
// update; dynamic bodies drive their nodes afterwards. collisionSteps must be at least one.
H3D_API bool h3dJoltStep(float deltaTime, int collisionSteps);

// Teleports a body to its bound node's current world transform.
H3D_API bool h3dJoltSyncBodyFromNode(H3DJoltBody body, bool activate);
H3D_API bool h3dJoltSetLinearVelocity(H3DJoltBody body, float x, float y, float z);
H3D_API bool h3dJoltSetAngularVelocity(H3DJoltBody body, float x, float y, float z);
H3D_API bool h3dJoltAddImpulse(H3DJoltBody body, float x, float y, float z);
H3D_API bool h3dJoltSetFriction(H3DJoltBody body, float friction);
H3D_API bool h3dJoltSetRestitution(H3DJoltBody body, float restitution);
H3D_API bool h3dJoltIsBodyActive(H3DJoltBody body);
