#include "../catch.hpp"
#include "Horde3D.h"

#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#define NULL_RENDER_BACKEND 256

namespace {

std::vector<char> readFile(const std::string &path)
{
	std::ifstream input(path, std::ios::binary);
	return input ? std::vector<char>(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()) : std::vector<char>();
}

bool loadFile(H3DRes resource, const std::string &path)
{
	const std::vector<char> bytes = readFile(path);
	return !bytes.empty() && h3dLoadResource(resource, bytes.data(), static_cast<int>(bytes.size()));
}

class GltfEngineSession
{
public:
	GltfEngineSession() : initialized(h3dInit((H3DRenderDevice::List)NULL_RENDER_BACKEND)) {}
	~GltfEngineSession() { if(initialized) h3dRelease(); }
	bool initialized;
};

}

TEST_CASE("GLTFConv output loads with node and morph animation", "[unit-gltf]")
{
	GltfEngineSession session;
	REQUIRE(session.initialized);
	const std::string content = H3D_SOURCE_CONTENT_DIR;
	const std::string resourceBase = "gltf/buster_drone/buster_drone";
	const std::string fileBase = content + "/" + resourceBase;

	H3DRes geometry = h3dAddResource(H3DResTypes::Geometry, (resourceBase + ".geo").c_str(), 0);
	REQUIRE(loadFile(geometry, fileBase + ".geo"));
	REQUIRE(h3dGetResParamI(geometry, H3DGeoRes::GeometryElem, 0, H3DGeoRes::GeoVertexCountI) > 0);
	REQUIRE(h3dGetResParamI(geometry, H3DGeoRes::GeometryElem, 0, H3DGeoRes::GeoMorphTargetCountI) == 2);

	H3DRes animation = h3dAddResource(H3DResTypes::Animation, (resourceBase + ".anim").c_str(), 0);
	REQUIRE(loadFile(animation, fileBase + ".anim"));
	REQUIRE(h3dGetResElemCount(animation, H3DAnimRes::EntityElem) == 34);
	REQUIRE(h3dGetResParamI(animation, H3DAnimRes::EntityElem, 0, H3DAnimRes::EntFrameCountI) == 751);

	H3DRes scene = h3dAddResource(H3DResTypes::SceneGraph, (resourceBase + ".scene.xml").c_str(), 0);
	REQUIRE(loadFile(scene, fileBase + ".scene.xml"));
	H3DNode model = h3dAddNodes(H3DRootNode, scene);
	REQUIRE(model != 0);
	REQUIRE(h3dSetModelMorpher(model, "1_Target_0", 0.25f));
	REQUIRE(h3dSetModelMorpher(model, "1_Target_1", 0.75f));
	h3dSetupModelAnimStage(model, 0, animation, 0, "", false);
	h3dSetModelAnimParams(model, 0, 300.0f, 1.0f);
	h3dUpdateModel(model, H3DModelUpdateFlags::Animation | H3DModelUpdateFlags::Geometry);
	REQUIRE_FALSE(h3dGetError());
}
