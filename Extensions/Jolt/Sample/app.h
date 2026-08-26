// Horde3D Jolt Physics sample
// This sample may be used without restriction; Horde3D's warranty disclaimer applies.

#pragma once

#include "../../../Horde3D/Samples/Framework/sampleapp.h"
#include "Horde3DJolt.h"
#ifdef H3D_JOLT_SAMPLE_HDRI
#include "Horde3DHDRI.h"
#endif
#include <vector>

class JoltPhysicsSample : public SampleApplication
{
public:
	JoltPhysicsSample(int argc, char **argv);

protected:
	bool initResources() override;
	void releaseResources() override;
	void update() override;
	void render() override;
	void keyEventHandler(int key, int keyState, int mods) override;

private:
	H3DNode createBox(const char *name, float x, float y, float z,
		float sizeX, float sizeY, float sizeZ, int motionType, float mass, H3DRes material);
	H3DNode createSphere(const char *name, float x, float y, float z, float radius, float mass);
	void createSimulationScene();
	void clearSimulationScene();
	void applyIblState();

	H3DRes _cubeGeometry = 0;
	H3DRes _boxMaterial = 0;
	H3DRes _floorMaterial = 0;
	H3DRes _sphereScene = 0;
	H3DRes _skyboxScene = 0;
	H3DNode _skyboxNode = 0;
#ifdef H3D_JOLT_SAMPLE_HDRI
	H3DRes _hdriSource = 0;
	H3DHdriIbl _ibl;
#endif
	H3DNode _kinematicPlatform = 0;
	H3DJoltBody _impulseBody = 0;
	std::vector<H3DNode> _physicsNodes;
	std::vector<H3DJoltBody> _physicsBodies;
	float _accumulator = 0.0f;
	float _simulationTime = 0.0f;
	float _nextAutoImpulseTime = 2.0f;
	bool _iblEnabled = true;
};
