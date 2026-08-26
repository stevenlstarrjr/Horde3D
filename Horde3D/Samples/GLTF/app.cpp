#include "app.h"

#include "FrameworkBackend.h"
#include "Horde3D.h"
#include "Horde3DOverlays.h"
#include "Horde3DUtils.h"

#include <algorithm>
#include <cstring>

GLTFViewer::GLTFViewer(int argc, char **argv) :
	SampleApplication(argc, argv, "glTF 2.0 - Horde3D Sample", 45.0f, 0.05f, 500.0f)
{
	_x = 0.0f; _y = 3.0f; _z = 10.5f;
	_rx = 0.0f; _ry = 0.0f;
	_curPipeline = 2;
	showStatPanel(1);
}

bool GLTFViewer::initResources()
{
	_hdriSource = h3dAddResource(H3DResTypes::Texture, "hdri/citrus_orchard_road_puresky_4k.hdr",
		H3DResFlags::NoTexCompression | H3DResFlags::NoTexMipmaps);
	if(!SampleApplication::initResources()) return false;
	H3DRes scene = h3dAddResource(H3DResTypes::SceneGraph,
		"gltf/AnisotropyStrengthTest/AnisotropyStrengthTest.scene.xml", 0);
	H3DRes skyboxScene = h3dAddResource(H3DResTypes::SceneGraph, "models/skybox/skybox.scene.xml", 0);
	H3DRes lightMaterial = h3dAddResource(H3DResTypes::Material, "materials/light.material.xml", 0);
	if(!getBackend()->loadResources(getResourcePath())) { h3dutDumpMessages(); return false; }

	H3DHdriSettings settings;
	settings.environmentSize = 512; settings.irradianceSize = 32; settings.specularSize = 128;
	settings.brdfLutSize = 128; settings.sampleCount = 128; settings.lightingSaturation = 1.0f;
	if(!h3dHdriCreateIbl(_hdriSource, "GLTFCitrusOrchard", &settings, &_ibl)) return false;
	const char *materialPrefix = "gltf/AnisotropyStrengthTest/AnisotropyStrengthTest_";
	for(H3DRes material = h3dGetNextResource(H3DResTypes::Material, 0); material != 0;
		material = h3dGetNextResource(H3DResTypes::Material, material))
	{
		const char *name = h3dGetResName(material);
		if(name && std::strncmp(name, materialPrefix, std::strlen(materialPrefix)) == 0)
		{
			_materials.push_back(material);
			if(!h3dHdriBindIbl(material, &_ibl)) return false;
			// glTF PBR keeps four active specular samplers to stay within Horde3D's
			// 16-slot limit. Preserve the fully filtered roughness-1 endpoint.
			const int roughestSampler = h3dFindResElem(material, H3DMatRes::SamplerElem,
				H3DMatRes::SampNameStr, "iblSpecular3");
			if(roughestSampler >= 0)
				h3dSetResParamI(material, H3DMatRes::SamplerElem, roughestSampler,
					H3DMatRes::SampTexResI, _ibl.specular[4]);
		}
	}

	_cam = h3dAddCameraNode(H3DRootNode, "Camera", getPipelineRes());
	H3DNode skybox = h3dAddNodes(H3DRootNode, skyboxScene);
	if(skybox != 0) h3dSetNodeTransform(skybox, 0, 0, 0, 0, 0, 0, 100, 100, 100);
	H3DRes skyboxMaterial = h3dFindResource(H3DResTypes::Material, "models/skybox/skybox.material.xml");
	if(skyboxMaterial != 0) h3dSetResParamI(skyboxMaterial, H3DMatRes::SamplerElem, 0, H3DMatRes::SampTexResI, _ibl.environment);

	_model = h3dAddNodes(H3DRootNode, scene);
	if(_model == 0) return false;

	H3DNode keyLight = h3dAddLightNode(H3DRootNode, "Studio key light", lightMaterial, "LIGHTING", "SHADOWMAP");
	h3dSetNodeTransform(keyLight, 4.0f, 8.0f, 10.0f, -35.0f, 20.0f, 0.0f, 1, 1, 1);
	h3dSetNodeParamF(keyLight, H3DLight::RadiusF, 0, 40.0f);
	h3dSetNodeParamF(keyLight, H3DLight::FovF, 0, 100.0f);
	h3dSetNodeParamI(keyLight, H3DLight::ShadowMapCountI, 1);
	h3dSetNodeParamF(keyLight, H3DLight::ShadowMapBiasF, 0, 0.002f);
	h3dSetNodeParamF(keyLight, H3DLight::ColorF3, 0, 1.0f);
	h3dSetNodeParamF(keyLight, H3DLight::ColorF3, 1, 0.88f);
	h3dSetNodeParamF(keyLight, H3DLight::ColorF3, 2, 0.72f);
	h3dSetNodeParamF(keyLight, H3DLight::ColorMultiplierF, 0, 0.0f);

	H3DRes post = h3dFindResource(H3DResTypes::Material, "pipelines/postHDR.material.xml");
	if(post != 0)
	{
		h3dSetMaterialUniform(post, "hdrExposure", 0.30f, 0, 0, 0);
		h3dSetMaterialUniform(post, "hdrBrightThres", 1.0f, 0, 0, 0);
		h3dSetMaterialUniform(post, "hdrBrightOffset", 0.12f, 0, 0, 0);
		h3dSetMaterialUniform(post, "hdrBloomStrength", 0.14f, 0, 0, 0);
	}
	return true;
}

void GLTFViewer::releaseResources()
{
	h3dHdriReleaseIbl(&_ibl);
	SampleApplication::releaseResources();
}

void GLTFViewer::update()
{
	SampleApplication::update();
}

void GLTFViewer::render()
{
	static const char *debugNames[] = { "Shaded", "Metallic", "Roughness", "Normals", "Occlusion" };
	h3dShowText("glTF 2.0: KHR_materials_anisotropy strength test", 0.03f, 0.89f, 0.028f,
		0.35f, 1.0f, 0.65f, _fontMatRes);
	std::string mode = std::string("F8 PBR view: ") + debugNames[_pbrDebugView];
	h3dShowText(mode.c_str(), 0.03f, 0.94f, 0.022f,
		1.0f, 1.0f, 1.0f, _fontMatRes);
	SampleApplication::render();
}

void GLTFViewer::keyEventHandler(int key, int keyState, int mods)
{
	SampleApplication::keyEventHandler(key, keyState, mods);
	if(key != KEY_F8 || keyState != KEY_PRESS) return;
	_pbrDebugView = (_pbrDebugView + 1) % 5;
	for(H3DRes material : _materials)
		if(material != 0) h3dSetMaterialUniform(material, "pbrDebugView", static_cast<float>(_pbrDebugView), 0, 0, 0);
}
