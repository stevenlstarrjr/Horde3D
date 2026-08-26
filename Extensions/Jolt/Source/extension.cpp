// Horde3D Jolt Physics Extension
// SPDX-License-Identifier: EPL-1.0

#include "extension.h"
#include "physics.h"
#include "utPlatform.h"

namespace Horde3DJolt {

bool ExtJolt::init()
{
	return physicsWorld().init();
}

void ExtJolt::release()
{
	physicsWorld().release();
}

H3D_IMPL void h3dJoltSetGravity(float x, float y, float z)
{
	physicsWorld().setGravity(x, y, z);
}

H3D_IMPL int h3dJoltAddBoxBody(int node, float halfX, float halfY, float halfZ,
	int motionType, float mass)
{
	return physicsWorld().addBox(node, halfX, halfY, halfZ, motionType, mass);
}

H3D_IMPL int h3dJoltAddSphereBody(int node, float radius, int motionType, float mass)
{
	return physicsWorld().addSphere(node, radius, motionType, mass);
}

H3D_IMPL int h3dJoltAddCapsuleBody(int node, float halfHeight, float radius,
	int motionType, float mass)
{
	return physicsWorld().addCapsule(node, halfHeight, radius, motionType, mass);
}

H3D_IMPL bool h3dJoltRemoveBody(int body)
{
	return physicsWorld().removeBody(body);
}

H3D_IMPL int h3dJoltGetNodeBody(int node)
{
	return physicsWorld().getNodeBody(node);
}

H3D_IMPL bool h3dJoltStep(float deltaTime, int collisionSteps)
{
	return physicsWorld().step(deltaTime, collisionSteps);
}

H3D_IMPL bool h3dJoltSyncBodyFromNode(int body, bool activate)
{
	return physicsWorld().syncBodyFromNode(body, activate);
}

H3D_IMPL bool h3dJoltSetLinearVelocity(int body, float x, float y, float z)
{
	return physicsWorld().setLinearVelocity(body, x, y, z);
}

H3D_IMPL bool h3dJoltSetAngularVelocity(int body, float x, float y, float z)
{
	return physicsWorld().setAngularVelocity(body, x, y, z);
}

H3D_IMPL bool h3dJoltAddImpulse(int body, float x, float y, float z)
{
	return physicsWorld().addImpulse(body, x, y, z);
}

H3D_IMPL bool h3dJoltSetFriction(int body, float friction)
{
	return physicsWorld().setFriction(body, friction);
}

H3D_IMPL bool h3dJoltSetRestitution(int body, float restitution)
{
	return physicsWorld().setRestitution(body, restitution);
}

H3D_IMPL bool h3dJoltIsBodyActive(int body)
{
	return physicsWorld().isBodyActive(body);
}

}
