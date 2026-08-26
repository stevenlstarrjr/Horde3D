// Horde3D Jolt Physics Extension
// SPDX-License-Identifier: EPL-1.0

#pragma once

#include "egPrerequisites.h"
#include <memory>

namespace JPH { class Shape; }

namespace Horde3DJolt {

class PhysicsWorld
{
public:
	PhysicsWorld();
	~PhysicsWorld();

	bool init();
	void release();
	bool isInitialized() const;

	void setGravity(float x, float y, float z);
	int addBox(Horde3D::NodeHandle node, float halfX, float halfY, float halfZ, int motionType, float mass);
	int addSphere(Horde3D::NodeHandle node, float radius, int motionType, float mass);
	int addCapsule(Horde3D::NodeHandle node, float halfHeight, float radius, int motionType, float mass);
	bool removeBody(int body);
	int getNodeBody(Horde3D::NodeHandle node) const;
	bool step(float deltaTime, int collisionSteps);
	bool syncBodyFromNode(int body, bool activate);
	bool setLinearVelocity(int body, float x, float y, float z);
	bool setAngularVelocity(int body, float x, float y, float z);
	bool addImpulse(int body, float x, float y, float z);
	bool setFriction(int body, float friction);
	bool setRestitution(int body, float restitution);
	bool isBodyActive(int body) const;

private:
	struct Impl;
	std::unique_ptr<Impl> _impl;
	int addBody(Horde3D::NodeHandle node, const JPH::Shape *shape, int motionType, float mass);
};

PhysicsWorld &physicsWorld();

}
