// Horde3D Jolt Physics sample
// This sample may be used without restriction; Horde3D's warranty disclaimer applies.

#include "app.h"

#include "FrameworkBackend.h"
#include "Horde3D.h"
#include "Horde3DOverlays.h"
#include "Horde3DUtils.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace {

H3DRes createCubeGeometry()
{
	float positions[] = {
		-0.5f,-0.5f, 0.5f,  0.5f,-0.5f, 0.5f,  0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f,
		 0.5f,-0.5f,-0.5f, -0.5f,-0.5f,-0.5f, -0.5f, 0.5f,-0.5f,  0.5f, 0.5f,-0.5f,
		 0.5f,-0.5f, 0.5f,  0.5f,-0.5f,-0.5f,  0.5f, 0.5f,-0.5f,  0.5f, 0.5f, 0.5f,
		-0.5f,-0.5f,-0.5f, -0.5f,-0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f,-0.5f,
		-0.5f, 0.5f, 0.5f,  0.5f, 0.5f, 0.5f,  0.5f, 0.5f,-0.5f, -0.5f, 0.5f,-0.5f,
		-0.5f,-0.5f,-0.5f,  0.5f,-0.5f,-0.5f,  0.5f,-0.5f, 0.5f, -0.5f,-0.5f, 0.5f
	};
	unsigned int indices[] = {
		 0, 1, 2,  0, 2, 3,  4, 5, 6,  4, 6, 7,
		 8, 9,10,  8,10,11, 12,13,14, 12,14,15,
		16,17,18, 16,18,19, 20,21,22, 20,22,23
	};
	short normals[] = {
		0,0,32767, 0,0,32767, 0,0,32767, 0,0,32767,
		0,0,-32767, 0,0,-32767, 0,0,-32767, 0,0,-32767,
		32767,0,0, 32767,0,0, 32767,0,0, 32767,0,0,
		-32767,0,0, -32767,0,0, -32767,0,0, -32767,0,0,
		0,32767,0, 0,32767,0, 0,32767,0, 0,32767,0,
		0,-32767,0, 0,-32767,0, 0,-32767,0, 0,-32767,0
	};
	float texCoords[] = {
		0,0, 1,0, 1,1, 0,1, 0,0, 1,0, 1,1, 0,1,
		0,0, 1,0, 1,1, 0,1, 0,0, 1,0, 1,1, 0,1,
		0,0, 1,0, 1,1, 0,1, 0,0, 1,0, 1,1, 0,1
	};
	return h3dutCreateGeometryRes("JoltCube", 24, 36, positions, indices, normals,
		nullptr, nullptr, texCoords, nullptr);
}

H3DRes createMaterial(const char *name, float red, float green, float blue,
	float metallic, float roughness)
{
#ifdef H3D_JOLT_SAMPLE_HDRI
	std::string material =
		"<Material>\n"
		"  <Shader source=\"shaders/pbr_ibl.shader\"/>\n"
		"  <Sampler name=\"iblIrradiance\" map=\"models/skybox/skybox.dds\"/>\n"
		"  <Sampler name=\"iblSpecular0\" map=\"models/skybox/skybox.dds\"/>\n"
		"  <Sampler name=\"iblSpecular1\" map=\"models/skybox/skybox.dds\"/>\n"
		"  <Sampler name=\"iblSpecular2\" map=\"models/skybox/skybox.dds\"/>\n"
		"  <Sampler name=\"iblSpecular3\" map=\"models/skybox/skybox.dds\"/>\n"
		"  <Sampler name=\"iblSpecular4\" map=\"models/skybox/skybox.dds\"/>\n"
		"  <Sampler name=\"iblBrdfLut\" map=\"textures/common/white.tga\"/>\n"
		"  <Uniform name=\"baseColor\" a=\"" + std::to_string(red) +
		"\" b=\"" + std::to_string(green) + "\" c=\"" + std::to_string(blue) + "\" d=\"1\"/>\n"
		"  <Uniform name=\"pbrParams\" a=\"" + std::to_string(metallic) +
		"\" b=\"" + std::to_string(roughness) + "\" c=\"1\" d=\"1\"/>\n"
		"</Material>";
#else
	std::string material =
		"<Material>\n"
		"  <Shader source=\"shaders/model.shader\"/>\n"
		"  <Uniform name=\"matDiffuseCol\" a=\"" + std::to_string(red) +
		"\" b=\"" + std::to_string(green) + "\" c=\"" + std::to_string(blue) + "\" d=\"1\"/>\n"
		"  <Uniform name=\"matSpecParams\" a=\"0.08\" b=\"0.08\" c=\"0.08\" d=\"0.35\"/>\n"
		"</Material>";
#endif
	H3DRes resource = h3dAddResource(H3DResTypes::Material, name, 0);
	if(!h3dLoadResource(resource, material.data(), static_cast<int>(material.size()))) return 0;
	return resource;
}

}

JoltPhysicsSample::JoltPhysicsSample(int argc, char **argv) :
	SampleApplication(argc, argv, "Jolt Physics - Horde3D Sample", 45.0f, 0.1f, 250.0f)
{
	_x = 11.0f; _y = 8.0f; _z = 18.0f;
	_rx = -15.0f; _ry = 32.0f;
	_helpRows += 3;
	showStatPanel(1);
#ifdef H3D_JOLT_SAMPLE_HDRI
	_curPipeline = 2;
#endif
}

bool JoltPhysicsSample::initResources()
{
#ifdef H3D_JOLT_SAMPLE_HDRI
	_hdriSource = h3dAddResource(H3DResTypes::Texture,
		"hdri/ferndale_studio_12_4k(1).hdr",
		H3DResFlags::NoTexCompression | H3DResFlags::NoTexMipmaps);
#endif
	if(!SampleApplication::initResources()) return false;
	if(!h3dCheckExtension("JoltPhysics")) return false;

#ifdef H3D_JOLT_SAMPLE_HDRI
	if(!h3dCheckExtension("HDRI")) return false;
	H3DHdriSettings iblSettings;
	iblSettings.environmentSize = 512;
	iblSettings.irradianceSize = 32;
	iblSettings.specularSize = 128;
	iblSettings.brdfLutSize = 128;
	iblSettings.sampleCount = 64;
	if(!h3dHdriCreateIbl(_hdriSource, "JoltFerndaleStudio", &iblSettings, &_ibl)) return false;
#endif

	H3DRes lightMaterial = h3dAddResource(H3DResTypes::Material, "materials/light.material.xml", 0);
	_sphereScene = h3dAddResource(H3DResTypes::SceneGraph, "models/sphere/sphere.scene.xml", 0);
	_skyboxScene = h3dAddResource(H3DResTypes::SceneGraph, "models/skybox/skybox.scene.xml", 0);
	_cubeGeometry = createCubeGeometry();
	_boxMaterial = createMaterial("JoltBoxMaterial", 0.95f, 0.32f, 0.08f, 0.18f, 0.3f);
	_floorMaterial = createMaterial("JoltFloorMaterial", 0.16f, 0.34f, 0.22f, 0.0f, 0.82f);
	if(_cubeGeometry == 0 || _boxMaterial == 0 || _floorMaterial == 0) return false;

#ifdef H3D_JOLT_SAMPLE_HDRI
	if(!h3dHdriBindIbl(_boxMaterial, &_ibl) || !h3dHdriBindIbl(_floorMaterial, &_ibl)) return false;
#endif

	if(!getBackend()->loadResources(getResourcePath()))
	{
		h3dutDumpMessages();
		return false;
	}

#ifdef H3D_JOLT_SAMPLE_HDRI
	H3DRes hdrMaterial = h3dFindResource(H3DResTypes::Material, "pipelines/postHDR.material.xml");
	if(hdrMaterial != 0)
	{
		h3dSetMaterialUniform(hdrMaterial, "hdrExposure", 0.55f, 0, 0, 0);
		h3dSetMaterialUniform(hdrMaterial, "hdrBrightThres", 2.0f, 0, 0, 0);
		h3dSetMaterialUniform(hdrMaterial, "hdrBrightOffset", 0.35f, 0, 0, 0);
		h3dSetMaterialUniform(hdrMaterial, "hdrBloomStrength", 0.08f, 0, 0, 0);
	}
#endif

	_cam = h3dAddCameraNode(H3DRootNode, "Camera", getPipelineRes());
	_skyboxNode = h3dAddNodes(H3DRootNode, _skyboxScene);
	if(_skyboxNode != 0) h3dSetNodeTransform(_skyboxNode, 0, 0, 0, 0, 0, 0, 80, 80, 80);
#ifdef H3D_JOLT_SAMPLE_HDRI
	H3DRes skyboxMaterial = h3dFindResource(H3DResTypes::Material, "models/skybox/skybox.material.xml");
	if(skyboxMaterial != 0)
		h3dSetResParamI(skyboxMaterial, H3DMatRes::SamplerElem, 0, H3DMatRes::SampTexResI, _ibl.environment);
#endif
	H3DNode light = h3dAddLightNode(H3DRootNode, "PhysicsLight", lightMaterial, "LIGHTING", "SHADOWMAP");
	h3dSetNodeTransform(light, 6, 14, 10, -55, 25, 0, 1, 1, 1);
	h3dSetNodeParamF(light, H3DLight::RadiusF, 0, 60.0f);
	h3dSetNodeParamF(light, H3DLight::FovF, 0, 100.0f);
	h3dSetNodeParamI(light, H3DLight::ShadowMapCountI, 1);
	h3dSetNodeParamF(light, H3DLight::ShadowMapBiasF, 0, 0.002f);
	h3dSetNodeParamF(light, H3DLight::ColorF3, 0, 1.0f);
	h3dSetNodeParamF(light, H3DLight::ColorF3, 1, 0.85f);
	h3dSetNodeParamF(light, H3DLight::ColorF3, 2, 0.7f);

	_helpLabels[_helpRows - 3] = "R:"; _helpValues[_helpRows - 3] = "Reset physics scene";
	_helpLabels[_helpRows - 2] = "F:"; _helpValues[_helpRows - 2] = "Impulse top body";
	_helpLabels[_helpRows - 1] = "I:"; _helpValues[_helpRows - 1] = "Toggle HDRI lighting";
	h3dJoltSetGravity(0.0f, -9.81f, 0.0f);
	createSimulationScene();
	return true;
}

H3DNode JoltPhysicsSample::createBox(const char *name, float x, float y, float z,
	float sizeX, float sizeY, float sizeZ, int motionType, float mass, H3DRes material)
{
	H3DNode model = h3dAddModelNode(H3DRootNode, name, _cubeGeometry);
	H3DNode mesh = h3dAddMeshNode(model, "CubeMesh", material, H3DMeshPrimType::TriangleList, 0, 36, 0, 23);
	if(model == 0 || mesh == 0) return 0;
	h3dSetNodeTransform(model, x, y, z, 0, 0, 0, sizeX, sizeY, sizeZ);
	H3DJoltBody body = h3dJoltAddBoxBody(model, sizeX * 0.5f, sizeY * 0.5f, sizeZ * 0.5f,
		motionType, mass);
	if(body == 0)
	{
		h3dRemoveNode(model);
		return 0;
	}
	_physicsNodes.push_back(model);
	_physicsBodies.push_back(body);
	return model;
}

H3DNode JoltPhysicsSample::createSphere(const char *name, float x, float y, float z,
	float radius, float mass)
{
	H3DNode node = h3dAddNodes(H3DRootNode, _sphereScene);
	if(node == 0) return 0;
	h3dSetNodeParamStr(node, H3DNodeParams::NameStr, name);
	h3dSetNodeTransform(node, x, y, z, 0, 0, 0, radius, radius, radius);
	H3DJoltBody body = h3dJoltAddSphereBody(node, radius, H3DJoltMotionType::Dynamic, mass);
	if(body == 0)
	{
		h3dRemoveNode(node);
		return 0;
	}
	_physicsNodes.push_back(node);
	_physicsBodies.push_back(body);
	return node;
}

void JoltPhysicsSample::createSimulationScene()
{
	createBox("Floor", 0, -0.25f, 0, 14, 0.5f, 14, H3DJoltMotionType::Static, 0, _floorMaterial);
	_kinematicPlatform = createBox("MovingPlatform", 0, 1.5f, -4, 4, 0.3f, 2,
		H3DJoltMotionType::Kinematic, 0, _floorMaterial);

	for(int i = 0; i < 8; ++i)
	{
		const float x = -3.4f + static_cast<float>(i % 4) * 1.35f;
		const float y = 4.0f + static_cast<float>(i / 4) * 2.2f + static_cast<float>(i % 2) * 0.6f;
		H3DNode box = createBox("FallingBox", x, y, -0.5f, 1, 1, 1,
			H3DJoltMotionType::Dynamic, 1.0f, _boxMaterial);
		H3DJoltBody body = h3dJoltGetNodeBody(box);
		h3dJoltSetAngularVelocity(body, 0.35f * i, 0.2f, -0.25f * i);
		h3dJoltSetRestitution(body, 0.25f);
	}

	for(int i = 0; i < 6; ++i)
	{
		H3DNode sphere = createSphere("BouncingSphere", 2.2f + 0.35f * (i % 2),
			3.5f + i * 1.25f, -0.2f + 0.25f * (i % 3), 0.55f, 1.0f);
		H3DJoltBody body = h3dJoltGetNodeBody(sphere);
		h3dJoltSetRestitution(body, 0.72f);
		_impulseBody = body;
	}

	for(int i = 0; i < 3; ++i)
		createBox("PlatformCargo", -1.0f + i, 3.0f + i * 1.1f, -4.0f, 0.8f, 0.8f, 0.8f,
			H3DJoltMotionType::Dynamic, 0.8f, _boxMaterial);

	if(!_physicsBodies.empty()) _impulseBody = _physicsBodies.back();
	_accumulator = 0.0f;
	_simulationTime = 0.0f;
	_nextAutoImpulseTime = 2.0f;
}

void JoltPhysicsSample::clearSimulationScene()
{
	for(H3DJoltBody body : _physicsBodies) h3dJoltRemoveBody(body);
	for(H3DNode node : _physicsNodes) h3dRemoveNode(node);
	_physicsBodies.clear();
	_physicsNodes.clear();
	_kinematicPlatform = 0;
	_impulseBody = 0;
}

void JoltPhysicsSample::releaseResources()
{
	clearSimulationScene();
#ifdef H3D_JOLT_SAMPLE_HDRI
	h3dHdriReleaseIbl(&_ibl);
	if(_hdriSource != 0) h3dRemoveResource(_hdriSource);
	_hdriSource = 0;
#endif
	SampleApplication::releaseResources();
}

void JoltPhysicsSample::applyIblState()
{
#ifdef H3D_JOLT_SAMPLE_HDRI
	const float intensity = _iblEnabled ? 1.0f : 0.0f;
	h3dSetMaterialUniform(_boxMaterial, "pbrParams", 0.18f, 0.3f, 1.0f, intensity);
	h3dSetMaterialUniform(_floorMaterial, "pbrParams", 0.0f, 0.82f, 1.0f, intensity);
#endif
}

void JoltPhysicsSample::update()
{
	SampleApplication::update();
	if(checkFlag(SampleApplication::FreezeMode) != 0) return;

	const float frameTime = std::min(1.0f / std::max(getFPS(), 1.0f), 0.1f);
	_simulationTime += frameTime;
	if(_kinematicPlatform != 0)
	{
		const float x = std::sin(_simulationTime * 0.8f) * 3.0f;
		const float rotation = std::sin(_simulationTime * 0.55f) * 12.0f;
		h3dSetNodeTransform(_kinematicPlatform, x, 1.5f, -4.0f, 0, rotation, 0, 4, 0.3f, 2);
	}
	if(_impulseBody != 0 && _simulationTime >= _nextAutoImpulseTime)
	{
		h3dJoltAddImpulse(_impulseBody, -3.5f, 7.0f, 2.0f);
		_nextAutoImpulseTime += 3.0f;
	}

	const float fixedStep = 1.0f / 60.0f;
	_accumulator += frameTime;
	int steps = 0;
	while(_accumulator >= fixedStep && steps < 6)
	{
		h3dJoltStep(fixedStep, 1);
		_accumulator -= fixedStep;
		++steps;
	}
	if(steps == 6) _accumulator = 0.0f;
}

void JoltPhysicsSample::render()
{
	h3dShowText("Jolt Physics: live rigid-body simulation", 0.03f, 0.84f, 0.032f,
		1.0f, 0.72f, 0.25f, _fontMatRes);
	h3dShowText("F: launch body   R: reset   I: compare IBL   Space: pause", 0.03f, 0.89f, 0.024f,
		1.0f, 1.0f, 1.0f, _fontMatRes);
#ifdef H3D_JOLT_SAMPLE_HDRI
	h3dShowText(_iblEnabled ? "HDRI LIGHTING: ON" : "HDRI LIGHTING: OFF",
		0.03f, 0.94f, 0.026f, _iblEnabled ? 0.35f : 1.0f,
		_iblEnabled ? 1.0f : 0.25f, 0.25f, _fontMatRes);
#endif
	SampleApplication::render();
}

void JoltPhysicsSample::keyEventHandler(int key, int keyState, int mods)
{
	SampleApplication::keyEventHandler(key, keyState, mods);
	if(keyState != KEY_PRESS) return;

	if(key == KEY_R)
	{
		clearSimulationScene();
		createSimulationScene();
	}
	else if(key == KEY_F && _impulseBody != 0)
	{
		h3dJoltAddImpulse(_impulseBody, 4.0f, 8.0f, 1.5f);
	}
#ifdef H3D_JOLT_SAMPLE_HDRI
	else if(key == KEY_I)
	{
		_iblEnabled = !_iblEnabled;
		applyIblState();
	}
#endif
}
