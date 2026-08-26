// Horde3D Jolt Physics Extension
// SPDX-License-Identifier: EPL-1.0

#include "physics.h"

#include "egCom.h"
#include "egModules.h"
#include "egScene.h"

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <algorithm>
#include <cmath>
#include <thread>
#include <unordered_map>
#include <vector>

namespace Horde3DJolt {

using Horde3D::Matrix4f;
using Horde3D::NodeHandle;
using Horde3D::SceneNode;
using Horde3D::Vec3f;

namespace {

namespace Layers {
	static constexpr JPH::ObjectLayer NonMoving = 0;
	static constexpr JPH::ObjectLayer Moving = 1;
	static constexpr JPH::ObjectLayer Count = 2;
}

namespace BroadPhaseLayers {
	static const JPH::BroadPhaseLayer NonMoving(0);
	static const JPH::BroadPhaseLayer Moving(1);
	static constexpr unsigned int Count = 2;
}

class ObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter
{
public:
	bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override
	{
		return a == Layers::Moving || b == Layers::Moving;
	}
};

class BroadPhaseLayerMap final : public JPH::BroadPhaseLayerInterface
{
public:
	BroadPhaseLayerMap()
	{
		_layers[Layers::NonMoving] = BroadPhaseLayers::NonMoving;
		_layers[Layers::Moving] = BroadPhaseLayers::Moving;
	}

	unsigned int GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::Count; }
	JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override
	{
		return _layers[layer];
	}

private:
	JPH::BroadPhaseLayer _layers[Layers::Count];
};

class ObjectVsBroadPhaseFilter final : public JPH::ObjectVsBroadPhaseLayerFilter
{
public:
	bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer broadPhase) const override
	{
		return layer == Layers::Moving || broadPhase == BroadPhaseLayers::Moving;
	}
};

float axisLength(const Matrix4f &matrix, int column)
{
	return std::sqrt(matrix.c[column][0] * matrix.c[column][0] +
		matrix.c[column][1] * matrix.c[column][1] + matrix.c[column][2] * matrix.c[column][2]);
}

JPH::Quat rotationFromMatrix(const Matrix4f &matrix)
{
	const float lx = axisLength(matrix, 0);
	const float ly = axisLength(matrix, 1);
	const float lz = axisLength(matrix, 2);
	if(lx <= 1.0e-7f || ly <= 1.0e-7f || lz <= 1.0e-7f) return JPH::Quat::sIdentity();

	JPH::Mat44 rotation(
		JPH::Vec4(matrix.c[0][0] / lx, matrix.c[0][1] / lx, matrix.c[0][2] / lx, 0.0f),
		JPH::Vec4(matrix.c[1][0] / ly, matrix.c[1][1] / ly, matrix.c[1][2] / ly, 0.0f),
		JPH::Vec4(matrix.c[2][0] / lz, matrix.c[2][1] / lz, matrix.c[2][2] / lz, 0.0f),
		JPH::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
	return rotation.GetQuaternion().Normalized();
}

JPH::RVec3 positionFromMatrix(const Matrix4f &matrix)
{
	return JPH::RVec3(matrix.c[3][0], matrix.c[3][1], matrix.c[3][2]);
}

Matrix4f matrixFromBody(JPH::RVec3Arg position, JPH::QuatArg rotation)
{
	JPH::Mat44 source = JPH::Mat44::sRotation(rotation);
	Matrix4f result;
	for(int column = 0; column < 3; ++column)
	{
		JPH::Vec3 axis = source.GetColumn3(static_cast<JPH::uint>(column));
		result.c[column][0] = axis.GetX();
		result.c[column][1] = axis.GetY();
		result.c[column][2] = axis.GetZ();
	}
	result.c[3][0] = static_cast<float>(position.GetX());
	result.c[3][1] = static_cast<float>(position.GetY());
	result.c[3][2] = static_cast<float>(position.GetZ());
	return result;
}

void applyLocalScale(Matrix4f &matrix, const Vec3f &scale)
{
	const float values[3] = { scale.x, scale.y, scale.z };
	for(int column = 0; column < 3; ++column)
	{
		const float length = axisLength(matrix, column);
		if(length <= 1.0e-7f) continue;
		const float factor = values[column] / length;
		matrix.c[column][0] *= factor;
		matrix.c[column][1] *= factor;
		matrix.c[column][2] *= factor;
	}
}

bool validMotionType(int motionType)
{
	return motionType >= 0 && motionType <= 2;
}

JPH::EMotionType toMotionType(int motionType)
{
	switch(motionType)
	{
		case 0: return JPH::EMotionType::Static;
		case 1: return JPH::EMotionType::Kinematic;
		default: return JPH::EMotionType::Dynamic;
	}
}

}

struct PhysicsWorld::Impl
{
	struct BodyRecord
	{
		JPH::BodyID id;
		NodeHandle node = 0;
		SceneNode *nodePtr = nullptr;
		int motionType = 0;
		Vec3f localScale = Vec3f(1.0f, 1.0f, 1.0f);
	};

	BroadPhaseLayerMap broadPhaseLayerMap;
	ObjectVsBroadPhaseFilter objectVsBroadPhaseFilter;
	ObjectLayerPairFilter objectLayerPairFilter;
	std::unique_ptr<JPH::TempAllocatorMalloc> tempAllocator;
	std::unique_ptr<JPH::JobSystemThreadPool> jobSystem;
	std::unique_ptr<JPH::PhysicsSystem> system;
	std::unordered_map<int, BodyRecord> bodies;
	std::unordered_map<NodeHandle, int> nodeBodies;
	int nextHandle = 1;
	bool initialized = false;
};

PhysicsWorld::PhysicsWorld() : _impl(new Impl()) {}
PhysicsWorld::~PhysicsWorld() { release(); }

bool PhysicsWorld::init()
{
	if(_impl->initialized) return true;

	JPH::RegisterDefaultAllocator();
	if(JPH::Factory::sInstance != nullptr)
	{
		Horde3D::Modules::log().writeError("JoltPhysics: another Jolt factory is already active");
		return false;
	}

	JPH::Factory::sInstance = new JPH::Factory();
	JPH::RegisterTypes();

	const unsigned int hardwareThreads = std::thread::hardware_concurrency();
	const int workerThreads = static_cast<int>(hardwareThreads > 1 ? hardwareThreads - 1 : 1);
	_impl->tempAllocator.reset(new JPH::TempAllocatorMalloc());
	_impl->jobSystem.reset(new JPH::JobSystemThreadPool(
		JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, workerThreads));
	_impl->system.reset(new JPH::PhysicsSystem());
	_impl->system->Init(65536, 0, 65536, 10240, _impl->broadPhaseLayerMap,
		_impl->objectVsBroadPhaseFilter, _impl->objectLayerPairFilter);
	_impl->initialized = true;
	Horde3D::Modules::log().writeInfo("JoltPhysics extension initialized");
	return true;
}

void PhysicsWorld::release()
{
	if(!_impl || !_impl->initialized) return;

	std::vector<int> handles;
	handles.reserve(_impl->bodies.size());
	for(const auto &entry : _impl->bodies) handles.push_back(entry.first);
	for(int handle : handles) removeBody(handle);

	_impl->system.reset();
	_impl->jobSystem.reset();
	_impl->tempAllocator.reset();
	JPH::UnregisterTypes();
	delete JPH::Factory::sInstance;
	JPH::Factory::sInstance = nullptr;
	_impl->initialized = false;
}

bool PhysicsWorld::isInitialized() const
{
	return _impl && _impl->initialized;
}

void PhysicsWorld::setGravity(float x, float y, float z)
{
	if(isInitialized()) _impl->system->SetGravity(JPH::Vec3(x, y, z));
}

int PhysicsWorld::addBox(NodeHandle node, float halfX, float halfY, float halfZ,
	int motionType, float mass)
{
	if(halfX <= 0.0f || halfY <= 0.0f || halfZ <= 0.0f) return 0;
	JPH::RefConst<JPH::Shape> shape = new JPH::BoxShape(JPH::Vec3(halfX, halfY, halfZ));
	return addBody(node, shape.GetPtr(), motionType, mass);
}

int PhysicsWorld::addSphere(NodeHandle node, float radius, int motionType, float mass)
{
	if(radius <= 0.0f) return 0;
	JPH::RefConst<JPH::Shape> shape = new JPH::SphereShape(radius);
	return addBody(node, shape.GetPtr(), motionType, mass);
}

int PhysicsWorld::addCapsule(NodeHandle node, float halfHeight, float radius,
	int motionType, float mass)
{
	if(halfHeight <= 0.0f || radius <= 0.0f) return 0;
	JPH::RefConst<JPH::Shape> shape = new JPH::CapsuleShape(halfHeight, radius);
	return addBody(node, shape.GetPtr(), motionType, mass);
}

int PhysicsWorld::addBody(NodeHandle node, const JPH::Shape *shape, int motionType, float mass)
{
	if(!isInitialized() || shape == nullptr || !validMotionType(motionType)) return 0;
	if(motionType == 2 && mass <= 0.0f) return 0;
	SceneNode *sceneNode = Horde3D::Modules::sceneMan().resolveNodeHandle(node);
	if(sceneNode == nullptr) return 0;
	auto existingNodeBody = _impl->nodeBodies.find(node);
	if(existingNodeBody != _impl->nodeBodies.end())
	{
		auto existingRecord = _impl->bodies.find(existingNodeBody->second);
		if(existingRecord != _impl->bodies.end() && existingRecord->second.nodePtr == sceneNode) return 0;
		removeBody(existingNodeBody->second);
	}
	Horde3D::Modules::sceneMan().updateNodes();
	const Matrix4f &worldTransform = sceneNode->getAbsTrans();

	JPH::BodyCreationSettings settings(shape, positionFromMatrix(worldTransform),
		rotationFromMatrix(worldTransform), toMotionType(motionType),
		motionType == 0 ? Layers::NonMoving : Layers::Moving);
	if(motionType == 2)
	{
		settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
		settings.mMassPropertiesOverride.mMass = mass;
	}

	const JPH::EActivation activation = motionType == 2 ? JPH::EActivation::Activate : JPH::EActivation::DontActivate;
	JPH::BodyID bodyID = _impl->system->GetBodyInterface().CreateAndAddBody(settings, activation);
	if(bodyID.IsInvalid()) return 0;

	int handle = _impl->nextHandle++;
	if(handle <= 0)
	{
		handle = 1;
		_impl->nextHandle = 2;
	}
	while(_impl->bodies.find(handle) != _impl->bodies.end()) handle = _impl->nextHandle++;

	Vec3f translation, rotation, scale;
	sceneNode->getTransform(translation, rotation, scale);
	Impl::BodyRecord record;
	record.id = bodyID;
	record.node = node;
	record.nodePtr = sceneNode;
	record.motionType = motionType;
	record.localScale = scale;
	_impl->bodies.emplace(handle, record);
	_impl->nodeBodies.emplace(node, handle);
	_impl->system->GetBodyInterface().SetUserData(bodyID, static_cast<JPH::uint64>(handle));
	return handle;
}

bool PhysicsWorld::removeBody(int body)
{
	if(!isInitialized()) return false;
	auto found = _impl->bodies.find(body);
	if(found == _impl->bodies.end()) return false;

	JPH::BodyInterface &bodyInterface = _impl->system->GetBodyInterface();
	bodyInterface.RemoveBody(found->second.id);
	bodyInterface.DestroyBody(found->second.id);
	_impl->nodeBodies.erase(found->second.node);
	_impl->bodies.erase(found);
	return true;
}

int PhysicsWorld::getNodeBody(NodeHandle node) const
{
	auto found = _impl->nodeBodies.find(node);
	if(found == _impl->nodeBodies.end()) return 0;
	auto record = _impl->bodies.find(found->second);
	SceneNode *sceneNode = Horde3D::Modules::sceneMan().resolveNodeHandle(node);
	return record != _impl->bodies.end() && record->second.nodePtr == sceneNode ? found->second : 0;
}

bool PhysicsWorld::step(float deltaTime, int collisionSteps)
{
	if(!isInitialized() || deltaTime <= 0.0f || collisionSteps < 1) return false;
	JPH::BodyInterface &bodyInterface = _impl->system->GetBodyInterface();
	std::vector<int> staleBodies;

	Horde3D::Modules::sceneMan().updateNodes();
	for(const auto &entry : _impl->bodies)
	{
		const Impl::BodyRecord &record = entry.second;
		SceneNode *node = Horde3D::Modules::sceneMan().resolveNodeHandle(record.node);
		if(node == nullptr || node != record.nodePtr)
		{
			staleBodies.push_back(entry.first);
			continue;
		}
		if(record.motionType == 1)
		{
			const Matrix4f &world = node->getAbsTrans();
			bodyInterface.MoveKinematic(record.id, positionFromMatrix(world),
				rotationFromMatrix(world), deltaTime);
		}
	}
	for(int handle : staleBodies) removeBody(handle);

	const JPH::EPhysicsUpdateError updateError = _impl->system->Update(deltaTime, collisionSteps,
		_impl->tempAllocator.get(), _impl->jobSystem.get());

	for(const auto &entry : _impl->bodies)
	{
		const Impl::BodyRecord &record = entry.second;
		if(record.motionType != 2) continue;
		SceneNode *node = Horde3D::Modules::sceneMan().resolveNodeHandle(record.node);
		if(node == nullptr || node != record.nodePtr) continue;

		JPH::RVec3 position;
		JPH::Quat rotation;
		bodyInterface.GetPositionAndRotation(record.id, position, rotation);
		Matrix4f localTransform = matrixFromBody(position, rotation);
		SceneNode *parent = node->getParent();
		if(parent != nullptr)
		{
			if(std::fabs(parent->getAbsTrans().determinant()) <= 1.0e-7f) continue;
			localTransform = parent->getAbsTrans().inverted() * localTransform;
		}
		applyLocalScale(localTransform, record.localScale);
		node->setTransform(localTransform);
	}
	return updateError == JPH::EPhysicsUpdateError::None;
}

bool PhysicsWorld::syncBodyFromNode(int body, bool activate)
{
	if(!isInitialized()) return false;
	auto found = _impl->bodies.find(body);
	if(found == _impl->bodies.end()) return false;
	SceneNode *node = Horde3D::Modules::sceneMan().resolveNodeHandle(found->second.node);
	if(node == nullptr || node != found->second.nodePtr) return false;
	Horde3D::Modules::sceneMan().updateNodes();
	const Matrix4f &world = node->getAbsTrans();
	_impl->system->GetBodyInterface().SetPositionAndRotation(found->second.id,
		positionFromMatrix(world), rotationFromMatrix(world),
		activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
	return true;
}

bool PhysicsWorld::setLinearVelocity(int body, float x, float y, float z)
{
	auto found = _impl->bodies.find(body);
	if(!isInitialized() || found == _impl->bodies.end() || found->second.motionType != 2) return false;
	_impl->system->GetBodyInterface().SetLinearVelocity(found->second.id, JPH::Vec3(x, y, z));
	return true;
}

bool PhysicsWorld::setAngularVelocity(int body, float x, float y, float z)
{
	auto found = _impl->bodies.find(body);
	if(!isInitialized() || found == _impl->bodies.end() || found->second.motionType != 2) return false;
	_impl->system->GetBodyInterface().SetAngularVelocity(found->second.id, JPH::Vec3(x, y, z));
	return true;
}

bool PhysicsWorld::addImpulse(int body, float x, float y, float z)
{
	auto found = _impl->bodies.find(body);
	if(!isInitialized() || found == _impl->bodies.end() || found->second.motionType != 2) return false;
	_impl->system->GetBodyInterface().AddImpulse(found->second.id, JPH::Vec3(x, y, z));
	return true;
}

bool PhysicsWorld::setFriction(int body, float friction)
{
	auto found = _impl->bodies.find(body);
	if(!isInitialized() || found == _impl->bodies.end() || friction < 0.0f) return false;
	_impl->system->GetBodyInterface().SetFriction(found->second.id, friction);
	return true;
}

bool PhysicsWorld::setRestitution(int body, float restitution)
{
	auto found = _impl->bodies.find(body);
	if(!isInitialized() || found == _impl->bodies.end() || restitution < 0.0f) return false;
	_impl->system->GetBodyInterface().SetRestitution(found->second.id, restitution);
	return true;
}

bool PhysicsWorld::isBodyActive(int body) const
{
	auto found = _impl->bodies.find(body);
	return isInitialized() && found != _impl->bodies.end() &&
		_impl->system->GetBodyInterface().IsActive(found->second.id);
}

PhysicsWorld &physicsWorld()
{
	static PhysicsWorld world;
	return world;
}

}
