#include "../catch.hpp"
#include "Horde3D.h"
#include "Horde3DHDRI.h"

#include <cstring>

#define NULL_RENDER_BACKEND 256

namespace {

class HdriEngineSession
{
public:
	HdriEngineSession() : initialized(h3dInit((H3DRenderDevice::List)NULL_RENDER_BACKEND)) {}
	~HdriEngineSession() { if(initialized) h3dRelease(); }
	bool initialized;
};

}

TEST_CASE("HDRI extension creates and binds a complete IBL set", "[unit-hdri]")
{
	HdriEngineSession session;
	REQUIRE(session.initialized);
	REQUIRE(h3dCheckExtension("HDRI"));

	H3DRes source = h3dCreateTexture("test.hdr", 8, 4, H3DFormats::TEX_RGBA32F,
		H3DResFlags::NoTexCompression | H3DResFlags::NoTexMipmaps);
	REQUIRE(source != 0);
	float *pixels = static_cast<float *>(h3dMapResStream(source, H3DTexRes::ImageElem, 0,
		H3DTexRes::ImgPixelStream, false, true));
	REQUIRE(pixels != nullptr);
	for(int i = 0; i < 8 * 4; ++i)
	{
		pixels[i * 4] = 0.2f + (i % 8) * 0.1f;
		pixels[i * 4 + 1] = 0.3f;
		pixels[i * 4 + 2] = 0.5f;
		pixels[i * 4 + 3] = 1.0f;
	}
	h3dUnmapResStream(source);

	H3DHdriSettings settings;
	settings.environmentSize = 4;
	settings.irradianceSize = 2;
	settings.specularSize = 4;
	settings.brdfLutSize = 4;
	settings.sampleCount = 4;
	H3DHdriIbl ibl;
	REQUIRE(h3dHdriCreateIbl(source, "TestIBL", &settings, &ibl));
	REQUIRE(ibl.environment != 0);
	REQUIRE(ibl.irradiance != 0);
	REQUIRE(ibl.brdfLut != 0);
	REQUIRE(h3dGetResParamI(ibl.environment, H3DTexRes::TextureElem, 0,
		H3DTexRes::TexSliceCountI) == 6);
	REQUIRE(h3dGetResParamI(ibl.environment, H3DTexRes::ImageElem, 0,
		H3DTexRes::ImgWidthI) == 4);
	REQUIRE(h3dGetResParamI(ibl.irradiance, H3DTexRes::ImageElem, 0,
		H3DTexRes::ImgWidthI) == 2);
	for(H3DRes resource : ibl.specular) REQUIRE(resource != 0);
	REQUIRE(h3dGetResParamI(ibl.brdfLut, H3DTexRes::TextureElem, 0,
		H3DTexRes::TexFormatI) == H3DFormats::TEX_RG32F);

	const char *materialXml =
		"<Material>"
		"<Sampler name=\"iblIrradiance\" map=\"dummyCube\"/>"
		"<Sampler name=\"iblSpecular0\" map=\"dummyCube\"/>"
		"<Sampler name=\"iblSpecular1\" map=\"dummyCube\"/>"
		"<Sampler name=\"iblSpecular2\" map=\"dummyCube\"/>"
		"<Sampler name=\"iblSpecular3\" map=\"dummyCube\"/>"
		"<Sampler name=\"iblSpecular4\" map=\"dummyCube\"/>"
		"<Sampler name=\"iblBrdfLut\" map=\"dummy2D\"/>"
		"</Material>";
	H3DRes material = h3dAddResource(H3DResTypes::Material, "testIbl.material.xml", 0);
	REQUIRE(h3dLoadResource(material, materialXml, static_cast<int>(std::strlen(materialXml))));
	REQUIRE(h3dHdriBindIbl(material, &ibl));
	REQUIRE(h3dGetResParamI(material, H3DMatRes::SamplerElem, 0,
		H3DMatRes::SampTexResI) == ibl.irradiance);
	REQUIRE(h3dGetResParamI(material, H3DMatRes::SamplerElem, 6,
		H3DMatRes::SampTexResI) == ibl.brdfLut);

	h3dHdriReleaseIbl(&ibl);
	REQUIRE(ibl.environment == 0);
	REQUIRE(ibl.irradiance == 0);
	REQUIRE(ibl.brdfLut == 0);
	for(H3DRes resource : ibl.specular) REQUIRE(resource == 0);
}
