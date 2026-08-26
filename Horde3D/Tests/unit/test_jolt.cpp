#include "../catch.hpp"
#include "Horde3D.h"
#include "Horde3DJolt.h"

#define NULL_RENDER_BACKEND 256

namespace {

class EngineSession
{
public:
	EngineSession() : initialized(h3dInit((H3DRenderDevice::List)NULL_RENDER_BACKEND)) {}
	~EngineSession() { if(initialized) h3dRelease(); }
	bool initialized;
};

}

TEST_CASE("Jolt dynamic bodies drive Horde node transforms", "[unit-jolt]")
{
	EngineSession session;
	REQUIRE(session.initialized);

	H3DNode floorNode = h3dAddGroupNode(H3DRootNode, "physics floor");
	h3dSetNodeTransform(floorNode, 0.0f, -0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
	H3DJoltBody floorBody = h3dJoltAddBoxBody(floorNode, 10.0f, 0.5f, 10.0f,
		H3DJoltMotionType::Static, 0.0f);
	REQUIRE(floorBody != 0);

	H3DNode sphereNode = h3dAddGroupNode(H3DRootNode, "physics sphere");
	h3dSetNodeTransform(sphereNode, 0.0f, 3.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
	H3DJoltBody sphereBody = h3dJoltAddSphereBody(sphereNode, 0.5f,
		H3DJoltMotionType::Dynamic, 1.0f);
	REQUIRE(sphereBody != 0);
	REQUIRE(h3dJoltGetNodeBody(sphereNode) == sphereBody);

	for(int i = 0; i < 180; ++i) REQUIRE(h3dJoltStep(1.0f / 60.0f, 1));

	float x, y, z;
	h3dGetNodeTransform(sphereNode, &x, &y, &z, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
	REQUIRE(x == Approx(0.0f).margin(0.05f));
	REQUIRE(y == Approx(0.5f).margin(0.08f));
	REQUIRE(z == Approx(0.0f).margin(0.05f));

	REQUIRE(h3dJoltRemoveBody(sphereBody));
	REQUIRE(h3dJoltRemoveBody(floorBody));
}
