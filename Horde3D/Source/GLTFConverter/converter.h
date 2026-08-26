// Horde3D glTF 2.0 converter
// SPDX-License-Identifier: EPL-1.0

#ifndef HORDE3D_GLTF_CONVERTER_H
#define HORDE3D_GLTF_CONVERTER_H

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct cgltf_accessor;
struct cgltf_data;
struct cgltf_image;
struct cgltf_material;
struct cgltf_mesh;
struct cgltf_node;
struct cgltf_primitive;

namespace Horde3D {
namespace GLTFConverter {

struct Vertex
{
	float position[3] = { 0, 0, 0 };
	float normal[3] = { 0, 0, 0 };
	float tangent[3] = { 0, 0, 0 };
	float bitangent[3] = { 0, 0, 0 };
	float uv0[2] = { 0, 0 };
	float uv1[2] = { 0, 0 };
};

struct MorphDiff
{
	uint32_t vertex = 0;
	float position[3] = { 0, 0, 0 };
	float normal[3] = { 0, 0, 0 };
	float tangent[3] = { 0, 0, 0 };
	float bitangent[3] = { 0, 0, 0 };
};

struct MorphTarget
{
	std::string name;
	std::vector<MorphDiff> diffs;
};

struct PrimitiveRange
{
	uint32_t first = 0;
	uint32_t count = 0;
	uint32_t vertexStart = 0;
	uint32_t vertexEnd = 0;
	std::string material;
};

class Converter
{
public:
	Converter();
	~Converter();

	bool load(const std::string &inputFile);
	bool write(const std::string &destination, const std::string &assetPath = std::string());
	const std::string &error() const { return _error; }
	std::string summary() const;

private:
	bool buildGeometry();
	bool appendPrimitive(const cgltf_mesh &mesh, const cgltf_primitive &primitive, PrimitiveRange &range);
	bool writeGeometry(const std::string &baseName);
	bool writeScene(const std::string &baseName, const std::string &assetPath);
	bool writeMaterials(const std::string &baseName, const std::string &assetPath);
	bool writeAnimations(const std::string &baseName, const std::string &assetPath, float framesPerSecond = 30.0f);
	bool writeNode(const cgltf_node &node, const std::string &baseName,
		const std::string &assetPath, unsigned depth, std::string &xml);
	std::string exportImage(const cgltf_image *image, const std::string &assetPath,
		const std::string &fallbackName);
	std::string materialName(const cgltf_material *material) const;
	std::string nodeName(const cgltf_node *node) const;
	std::string morphName(const cgltf_mesh *mesh, size_t targetIndex) const;
	void setError(const std::string &message);

	cgltf_data *_data = nullptr;
	std::string _inputFile;
	std::string _inputDirectory;
	std::string _destination;
	std::string _error;
	std::vector<Vertex> _vertices;
	std::vector<uint32_t> _indices;
	std::unordered_map<const cgltf_primitive *, PrimitiveRange> _ranges;
	std::unordered_map<const cgltf_image *, std::string> _exportedImages;
	std::vector<std::string> _nodeNames;
	std::unordered_set<const cgltf_node *> _animatedNodes;
	std::vector<MorphTarget> _morphTargets;
};

} // namespace GLTFConverter
} // namespace Horde3D

#endif
