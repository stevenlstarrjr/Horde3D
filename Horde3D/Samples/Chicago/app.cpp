// *************************************************************************************************
//
// Horde3D
//   Next-Generation Graphics Engine
//
// Sample Application
// --------------------------------------
// Copyright (C) 2006-2021 Nicolas Schulz and Horde3D team
//
//
// This sample source file is not covered by the EPL as the rest of the SDK
// and may be used without any restrictions. However, the EPL's disclaimer of
// warranty and liability shall be in effect for this file.
//
// *************************************************************************************************

#include "app.h"
#include "crowd.h"
#include "Horde3D.h"
#include "Horde3DUtils.h"
#include <math.h>
#include <iomanip>

#include "../Framework/FrameworkBackend.h"

using namespace std;


ChicagoSample::ChicagoSample( int argc, char** argv ) :
    SampleApplication( argc, argv, "Chicago - Horde3D Sample" ),
    _crowdSim(0)
{
    _x = 15; _y = 3; _z = 20;
    _rx = -10; _ry = 60;

	_curPipeline = 1;
	_useBinaryShaders = true;
	setRequiredCapabilities( RenderCapabilities::BinaryShaders );
}


bool ChicagoSample::initResources()
{
	_hdriSource = h3dAddResource(H3DResTypes::Texture,
		"hdri/citrus_orchard_road_puresky_4k.hdr",
		H3DResFlags::NoTexCompression | H3DResFlags::NoTexMipmaps);
	if ( !SampleApplication::initResources() )
        return false;
	if(!h3dCheckExtension("HDRI")) return false;

	// The original sample's three 2048px cascades dominate frame time on
	// integrated GPUs. One 1024px map preserves the nearby crowd shadows while
	// avoiding three complete additional draws of the 1.5M-triangle scene.
	h3dSetOption(H3DOptions::ShadowMapSize, 1024);

	H3DHdriSettings iblSettings;
	iblSettings.environmentSize = 512;
	iblSettings.irradianceSize = 32;
	iblSettings.specularSize = 128;
	iblSettings.brdfLutSize = 128;
	iblSettings.sampleCount = 64;
	iblSettings.lightingIntensity = 0.20f;
	if(!h3dHdriCreateIbl(_hdriSource, "ChicagoCitrusOrchard", &iblSettings, &_ibl))
		return false;

    // 1. Add resources

	// Shader for deferred shading
	H3DRes lightMatRes = h3dAddResource( H3DResTypes::Material, "materials/light.material.xml", 0 );

    // Environment
	H3DRes envRes = h3dAddResource( H3DResTypes::SceneGraph, "models/platform/platform.scene.xml", 0 );

    // Skybox
	H3DRes skyBoxRes = 0;
	// Chicago's deferred pipeline presents to an LDR framebuffer, so the visible
	// HDR background uses an ACES display transform. IBL remains true HDR.
	skyBoxRes = h3dAddResource(H3DResTypes::SceneGraph, "models/skybox/skybox_hdr.scene.xml", 0);

    // 2. Load resources

	if ( !getBackend()->loadResources( getResourcePath() ) )
    {
		h3dutDumpMessages();
        return false;
    }

	// Chicago's deferred ambient pass uses a single irradiance cubemap. Replace
	// the legacy baked map with the diffuse convolution of the real HDR image.
	H3DRes globalSettings = h3dFindResource(H3DResTypes::Material,
		"pipelines/globalSettings.material.xml");
	const int ambientSampler = globalSettings ? h3dFindResElem(globalSettings,
		H3DMatRes::SamplerElem, H3DMatRes::SampNameStr, "ambientMap") : -1;
	if(ambientSampler >= 0)
		h3dSetResParamI(globalSettings, H3DMatRes::SamplerElem, ambientSampler,
			H3DMatRes::SampTexResI, _ibl.irradiance);

    // 3. Add scene nodes

	// Add camera
	_cam = h3dAddCameraNode( H3DRootNode, "Camera", getPipelineRes() );
	//h3dSetNodeParamI( _cam, H3DCamera::OccCullingI, 1 );

    // Add environment
	H3DNode env = h3dAddNodes( H3DRootNode, envRes );
    h3dSetNodeTransform( env, 0, 0, 0, 0, 0, 0, 0.23f, 0.23f, 0.23f );

    // Add skybox
    H3DNode sky = h3dAddNodes( H3DRootNode, skyBoxRes );
	h3dSetNodeTransform( sky, 0, 0, 0, 0, 0, 0, 210, 50, 210 );
	h3dSetNodeFlags( sky, H3DNodeFlags::NoCastShadow, true );
	H3DRes skyboxMaterial = h3dFindResource(H3DResTypes::Material,
		"models/skybox/skybox_hdr.material.xml");
	if(skyboxMaterial != 0)
	{
		h3dSetResParamI(skyboxMaterial, H3DMatRes::SamplerElem, 0,
			H3DMatRes::SampTexResI, _ibl.environment);
		h3dSetMaterialUniform(skyboxMaterial, "hdrExposure", 0.18f, 0, 0, 0);
	}

    // Add light source
	H3DNode light = h3dAddLightNode( H3DRootNode, "Light1", lightMatRes, "LIGHTING", "SHADOWMAP" );
	h3dSetNodeTransform( light, 0, 20, 50, -30, 0, 0, 1, 1, 1 );
	h3dSetNodeParamF( light, H3DLight::RadiusF, 0, 200 );
	h3dSetNodeParamF( light, H3DLight::FovF, 0, 90 );
	h3dSetNodeParamI( light, H3DLight::ShadowMapCountI, 1 );
	h3dSetNodeParamF( light, H3DLight::ShadowSplitLambdaF, 0, 0.9f );
	h3dSetNodeParamF( light, H3DLight::ShadowMapBiasF, 0, 0.001f );
	h3dSetNodeParamF( light, H3DLight::ColorF3, 0, 0.9f );
	h3dSetNodeParamF( light, H3DLight::ColorF3, 1, 0.7f );
	h3dSetNodeParamF( light, H3DLight::ColorF3, 2, 0.75f );

    _crowdSim = new CrowdSim( getResourcePath() );
	_crowdSim->init( getBackend() );

	return true;
}


void ChicagoSample::releaseResources()
{
    delete _crowdSim;
    _crowdSim = 0x0;
	h3dHdriReleaseIbl(&_ibl);

    SampleApplication::releaseResources();
}


void ChicagoSample::update()
{
    SampleApplication::update();

    if( !checkFlag( SampleApplication::FreezeMode ) )
	{
        _crowdSim->update( getFPS() );
    }
}
