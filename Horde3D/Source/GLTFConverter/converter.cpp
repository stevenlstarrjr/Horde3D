// Horde3D glTF 2.0 converter
// SPDX-License-Identifier: EPL-1.0

#include "converter.h"

#include "utEndian.h"
#include "utMath.h"

#include <cgltf.h>
#include <webp/decode.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>

#if defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace Horde3D {
namespace GLTFConverter {
namespace {

template<class T> bool writeLE(FILE *file, const T *values, size_t count)
{
	unsigned char buffer[256];
	const size_t capacity = sizeof(buffer) / sizeof(T);
	while(count > 0)
	{
		const size_t amount = std::min(count, capacity);
		elemcpy_le(reinterpret_cast<T *>(buffer), values, amount);
		if(std::fwrite(buffer, sizeof(T), amount, file) != amount) return false;
		values += amount;
		count -= amount;
	}
	return true;
}

std::string directoryOf(const std::string &path)
{
	const size_t slash = path.find_last_of("/\\");
	return slash == std::string::npos ? std::string(".") : path.substr(0, slash);
}

std::string fileStem(const std::string &path)
{
	const size_t slash = path.find_last_of("/\\");
	const size_t begin = slash == std::string::npos ? 0 : slash + 1;
	const size_t dot = path.find_last_of('.');
	return path.substr(begin, dot == std::string::npos || dot < begin ? std::string::npos : dot - begin);
}

std::string fileName(const std::string &path)
{
	const size_t slash = path.find_last_of("/\\");
	return path.substr(slash == std::string::npos ? 0 : slash + 1);
}

std::string joinPath(const std::string &left, const std::string &right)
{
	if(left.empty() || left == ".") return left.empty() ? right : left + "/" + right;
	if(right.empty()) return left;
	return left.back() == '/' || left.back() == '\\' ? left + right : left + "/" + right;
}

bool makeDirectory(const std::string &path)
{
	if(path.empty() || path == ".") return true;
	std::string current;
	if(path[0] == '/') current = "/";
	for(size_t pos = 0; pos <= path.size();)
	{
		const size_t next = path.find_first_of("/\\", pos);
		const std::string part = path.substr(pos, next == std::string::npos ? std::string::npos : next - pos);
		if(!part.empty())
		{
			current = current.empty() || current.back() == '/' ? current + part : current + "/" + part;
#if defined(_WIN32)
			if(_mkdir(current.c_str()) != 0 && errno != EEXIST) return false;
#else
			if(::mkdir(current.c_str(), 0755) != 0 && errno != EEXIST) return false;
#endif
		}
		if(next == std::string::npos) break;
		pos = next + 1;
	}
	return true;
}

std::string sanitize(const char *value, const std::string &fallback)
{
	std::string result = value && value[0] ? value : fallback;
	for(char &c : result)
		if(!(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-')) c = '_';
	return result.empty() ? fallback : result;
}

std::string xmlEscape(const std::string &value)
{
	std::string result;
	for(char c : value)
	{
		switch(c)
		{
			case '&': result += "&amp;"; break;
			case '<': result += "&lt;"; break;
			case '>': result += "&gt;"; break;
			case '\"': result += "&quot;"; break;
			case '\'': result += "&apos;"; break;
			default: result += c; break;
		}
	}
	return result;
}

const cgltf_accessor *findAttribute(const cgltf_primitive &primitive, cgltf_attribute_type type, int index = 0)
{
	for(cgltf_size i = 0; i < primitive.attributes_count; ++i)
		if(primitive.attributes[i].type == type && primitive.attributes[i].index == index)
			return primitive.attributes[i].data;
	return nullptr;
}

const cgltf_accessor *findAttribute(const cgltf_morph_target &target, cgltf_attribute_type type)
{
	for(cgltf_size i = 0; i < target.attributes_count; ++i)
		if(target.attributes[i].type == type) return target.attributes[i].data;
	return nullptr;
}

void add3(float *a, const float *b) { a[0] += b[0]; a[1] += b[1]; a[2] += b[2]; }
void sub3(const float *a, const float *b, float *out) { out[0] = a[0]-b[0]; out[1] = a[1]-b[1]; out[2] = a[2]-b[2]; }
void cross3(const float *a, const float *b, float *out)
{
	out[0] = a[1]*b[2] - a[2]*b[1]; out[1] = a[2]*b[0] - a[0]*b[2]; out[2] = a[0]*b[1] - a[1]*b[0];
}
float dot3(const float *a, const float *b) { return a[0]*b[0] + a[1]*b[1] + a[2]*b[2]; }
void normalize3(float *value)
{
	const float length = std::sqrt(std::max(dot3(value, value), 1.0e-20f));
	value[0] /= length; value[1] /= length; value[2] /= length;
}

void makeFallbackTangent(Vertex &vertex)
{
	const float up[3] = { std::fabs(vertex.normal[1]) < 0.999f ? 0.0f : 1.0f,
		std::fabs(vertex.normal[1]) < 0.999f ? 1.0f : 0.0f, 0.0f };
	cross3(up, vertex.normal, vertex.tangent);
	normalize3(vertex.tangent);
	cross3(vertex.normal, vertex.tangent, vertex.bitangent);
	normalize3(vertex.bitangent);
}

void calculateBasis(std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices,
	bool normalsPresent, bool tangentsPresent)
{
	if(!normalsPresent)
		for(Vertex &vertex : vertices) std::fill(vertex.normal, vertex.normal + 3, 0.0f);
	if(!tangentsPresent)
		for(Vertex &vertex : vertices)
		{
			std::fill(vertex.tangent, vertex.tangent + 3, 0.0f);
			std::fill(vertex.bitangent, vertex.bitangent + 3, 0.0f);
		}

	for(size_t i = 0; i + 2 < indices.size(); i += 3)
	{
		Vertex &a = vertices[indices[i]], &b = vertices[indices[i+1]], &c = vertices[indices[i+2]];
		float edge1[3], edge2[3], face[3];
		sub3(b.position, a.position, edge1); sub3(c.position, a.position, edge2); cross3(edge1, edge2, face);
		if(!normalsPresent) { add3(a.normal, face); add3(b.normal, face); add3(c.normal, face); }
		if(!tangentsPresent)
		{
			const float du1 = b.uv0[0] - a.uv0[0], dv1 = b.uv0[1] - a.uv0[1];
			const float du2 = c.uv0[0] - a.uv0[0], dv2 = c.uv0[1] - a.uv0[1];
			const float determinant = du1 * dv2 - du2 * dv1;
			if(std::fabs(determinant) > 1.0e-12f)
			{
				const float inverse = 1.0f / determinant;
				float tangent[3], bitangent[3];
				for(int axis = 0; axis < 3; ++axis)
				{
					tangent[axis] = (edge1[axis] * dv2 - edge2[axis] * dv1) * inverse;
					bitangent[axis] = (edge2[axis] * du1 - edge1[axis] * du2) * inverse;
				}
				add3(a.tangent, tangent); add3(b.tangent, tangent); add3(c.tangent, tangent);
				add3(a.bitangent, bitangent); add3(b.bitangent, bitangent); add3(c.bitangent, bitangent);
			}
		}
	}

	for(Vertex &vertex : vertices)
	{
		normalize3(vertex.normal);
		if(dot3(vertex.tangent, vertex.tangent) < 1.0e-12f) makeFallbackTangent(vertex);
		else
		{
			normalize3(vertex.tangent);
			if(dot3(vertex.bitangent, vertex.bitangent) < 1.0e-12f) cross3(vertex.normal, vertex.tangent, vertex.bitangent);
			normalize3(vertex.bitangent);
		}
	}
}

short packedNormal(float value)
{
	return static_cast<short>(std::max(-1.0f, std::min(1.0f, value)) * 32767.0f);
}

std::string transformAttributes(const cgltf_node &node)
{
	float values[16];
	cgltf_node_transform_local(&node, values);
	Matrix4f matrix(values);
	Vec3f translation, rotation, scale;
	matrix.decompose(translation, rotation, scale);
	const float radiansToDegrees = 57.29577951308232f;
	std::ostringstream out;
	out << std::setprecision(9);
	if(translation != Vec3f(0,0,0)) out << " tx=\"" << translation.x << "\" ty=\"" << translation.y << "\" tz=\"" << translation.z << "\"";
	if(rotation != Vec3f(0,0,0)) out << " rx=\"" << rotation.x*radiansToDegrees << "\" ry=\"" << rotation.y*radiansToDegrees << "\" rz=\"" << rotation.z*radiansToDegrees << "\"";
	if(scale != Vec3f(1,1,1)) out << " sx=\"" << scale.x << "\" sy=\"" << scale.y << "\" sz=\"" << scale.z << "\"";
	return out.str();
}

bool copyFile(const std::string &source, const std::string &destination)
{
	std::ifstream input(source, std::ios::binary);
	if(!input) return false;
	std::ofstream output(destination, std::ios::binary);
	output << input.rdbuf();
	return output.good();
}

int base64Value(char c)
{
	if(c >= 'A' && c <= 'Z') return c - 'A';
	if(c >= 'a' && c <= 'z') return c - 'a' + 26;
	if(c >= '0' && c <= '9') return c - '0' + 52;
	if(c == '+') return 62;
	if(c == '/') return 63;
	return -1;
}

std::vector<unsigned char> decodeBase64(const char *text)
{
	std::vector<unsigned char> result;
	unsigned int accumulator = 0; int bits = 0;
	for(; *text && *text != '='; ++text)
	{
		const int value = base64Value(*text);
		if(value < 0) continue;
		accumulator = (accumulator << 6) | static_cast<unsigned int>(value); bits += 6;
		if(bits >= 8) { bits -= 8; result.push_back(static_cast<unsigned char>((accumulator >> bits) & 0xff)); }
	}
	return result;
}

std::string imageExtension(const cgltf_image &image)
{
	if(image.mime_type)
	{
		if(std::strcmp(image.mime_type, "image/png") == 0) return ".png";
		if(std::strcmp(image.mime_type, "image/jpeg") == 0) return ".jpg";
		if(std::strcmp(image.mime_type, "image/webp") == 0) return ".webp";
		if(std::strcmp(image.mime_type, "image/ktx2") == 0) return ".ktx2";
	}
	if(image.uri)
	{
		const std::string uri(image.uri);
		const size_t dot = uri.find_last_of('.');
		if(dot != std::string::npos && uri.find_first_of("?#", dot) == std::string::npos) return uri.substr(dot);
	}
	return ".bin";
}

bool isWebP(const cgltf_image &image)
{
	if(image.mime_type && std::strcmp(image.mime_type, "image/webp") == 0) return true;
	if(!image.uri) return false;
	std::string uri(image.uri);
	std::transform(uri.begin(), uri.end(), uri.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	const size_t dot = uri.find_last_of('.');
	return dot != std::string::npos && uri.compare(dot, 5, ".webp") == 0;
}

const cgltf_image *textureImage(const cgltf_texture_view &view)
{
	const cgltf_texture *texture = view.texture;
	if(!texture) return nullptr;
	if(texture->image) return texture->image;
	if(texture->has_webp) return texture->webp_image;
	if(texture->has_basisu) return texture->basisu_image;
	return nullptr;
}

struct TextureTransform
{
	float offset[2] = { 0.0f, 0.0f };
	float scale[2] = { 1.0f, 1.0f };
	float rotation = 0.0f;
};

TextureTransform textureTransform(const cgltf_texture_view *view)
{
	TextureTransform result;
	if(view && view->has_transform)
	{
		std::copy(view->transform.offset, view->transform.offset + 2, result.offset);
		std::copy(view->transform.scale, view->transform.scale + 2, result.scale);
		result.rotation = view->transform.rotation;
	}
	return result;
}

bool readFile(const std::string &path, std::vector<unsigned char> &bytes)
{
	std::ifstream input(path, std::ios::binary | std::ios::ate);
	if(!input) return false;
	const std::streamoff size = input.tellg();
	if(size <= 0) return false;
	bytes.resize(static_cast<size_t>(size));
	input.seekg(0, std::ios::beg);
	input.read(reinterpret_cast<char *>(bytes.data()), size);
	return input.good();
}

bool writeWebPAsTga(const std::vector<unsigned char> &webp, const std::string &path)
{
	int width = 0, height = 0;
	if(!WebPGetInfo(webp.data(), webp.size(), &width, &height) || width <= 0 || height <= 0) return false;
	uint8_t *pixels = WebPDecodeRGBA(webp.data(), webp.size(), &width, &height);
	if(!pixels) return false;
	for(size_t i = 0, count = static_cast<size_t>(width) * static_cast<size_t>(height); i < count; ++i)
		std::swap(pixels[i * 4], pixels[i * 4 + 2]);
	unsigned char header[18] = {};
	header[2] = 2;
	header[12] = static_cast<unsigned char>(width & 0xff); header[13] = static_cast<unsigned char>((width >> 8) & 0xff);
	header[14] = static_cast<unsigned char>(height & 0xff); header[15] = static_cast<unsigned char>((height >> 8) & 0xff);
	header[16] = 32; header[17] = 0x28;
	std::ofstream output(path, std::ios::binary);
	output.write(reinterpret_cast<const char *>(header), sizeof(header));
	output.write(reinterpret_cast<const char *>(pixels), static_cast<std::streamsize>(width) * height * 4);
	WebPFree(pixels);
	return output.good();
}

struct AnimationFrame
{
	float rotation[4] = { 0, 0, 0, 1 };
	float translation[3] = { 0, 0, 0 };
	float scale[3] = { 1, 1, 1 };
};

AnimationFrame defaultFrame(const cgltf_node &node)
{
	AnimationFrame frame;
	if(node.has_translation) std::copy(node.translation, node.translation + 3, frame.translation);
	if(node.has_rotation) std::copy(node.rotation, node.rotation + 4, frame.rotation);
	if(node.has_scale) std::copy(node.scale, node.scale + 3, frame.scale);
	if(node.has_matrix)
	{
		Matrix4f matrix(node.matrix);
		Vec3f translation, euler, scale;
		matrix.decompose(translation, euler, scale);
		Quaternion rotation(euler.x, euler.y, euler.z);
		frame.translation[0] = translation.x; frame.translation[1] = translation.y; frame.translation[2] = translation.z;
		frame.rotation[0] = rotation.x; frame.rotation[1] = rotation.y; frame.rotation[2] = rotation.z; frame.rotation[3] = rotation.w;
		frame.scale[0] = scale.x; frame.scale[1] = scale.y; frame.scale[2] = scale.z;
	}
	return frame;
}

bool readAccessorVector(const cgltf_accessor *accessor, cgltf_size index, float *value, cgltf_size components)
{
	return accessor && index < accessor->count && cgltf_accessor_read_float(accessor, index, value, components);
}

bool sampleChannel(const cgltf_animation_channel &channel, float time, float *value, cgltf_size components)
{
	const cgltf_animation_sampler &sampler = *channel.sampler;
	const cgltf_accessor *times = sampler.input;
	const cgltf_accessor *output = sampler.output;
	if(!times || !output || times->count == 0) return false;
	float firstTime = 0, lastTime = 0;
	if(!readAccessorVector(times, 0, &firstTime, 1) || !readAccessorVector(times, times->count - 1, &lastTime, 1)) return false;
	cgltf_size left = 0, right = times->count - 1;
	if(time <= firstTime) right = 0;
	else if(time >= lastTime) left = right;
	else
	{
		while(right - left > 1)
		{
			const cgltf_size middle = left + (right - left) / 2;
			float middleTime = 0;
			if(!readAccessorVector(times, middle, &middleTime, 1)) return false;
			if(time < middleTime) right = middle; else left = middle;
		}
	}
	float leftTime = firstTime, rightTime = lastTime;
	if(!readAccessorVector(times, left, &leftTime, 1) || !readAccessorVector(times, right, &rightTime, 1)) return false;
	const float amount = right == left ? 0.0f : (time - leftTime) / std::max(rightTime - leftTime, 1.0e-20f);
	const bool cubic = sampler.interpolation == cgltf_interpolation_type_cubic_spline;
	const cgltf_size leftValue = cubic ? left * 3 + 1 : left;
	const cgltf_size rightValue = cubic ? right * 3 + 1 : right;
	float a[4] = {0,0,0,0}, b[4] = {0,0,0,0};
	if(!readAccessorVector(output, leftValue, a, components) || !readAccessorVector(output, rightValue, b, components)) return false;
	if(right == left || sampler.interpolation == cgltf_interpolation_type_step)
	{
		std::copy(a, a + components, value); return true;
	}
	if(cubic)
	{
		float outTangent[4] = {0,0,0,0}, inTangent[4] = {0,0,0,0};
		if(!readAccessorVector(output, left * 3 + 2, outTangent, components) ||
			!readAccessorVector(output, right * 3, inTangent, components)) return false;
		const float t2 = amount * amount, t3 = t2 * amount, delta = rightTime - leftTime;
		const float h00 = 2*t3 - 3*t2 + 1, h10 = t3 - 2*t2 + amount;
		const float h01 = -2*t3 + 3*t2, h11 = t3 - t2;
		for(cgltf_size i = 0; i < components; ++i)
			value[i] = h00*a[i] + h10*delta*outTangent[i] + h01*b[i] + h11*delta*inTangent[i];
	}
	else if(channel.target_path == cgltf_animation_path_type_rotation)
	{
		Quaternion qa(a[0], a[1], a[2], a[3]), qb(b[0], b[1], b[2], b[3]);
		const Quaternion q = qa.slerp(qb, amount);
		value[0] = q.x; value[1] = q.y; value[2] = q.z; value[3] = q.w;
	}
	else
		for(cgltf_size i = 0; i < components; ++i) value[i] = a[i] + (b[i] - a[i]) * amount;
	if(channel.target_path == cgltf_animation_path_type_rotation)
	{
		const float length = std::sqrt(std::max(value[0]*value[0] + value[1]*value[1] + value[2]*value[2] + value[3]*value[3], 1.0e-20f));
		for(int i = 0; i < 4; ++i) value[i] /= length;
	}
	return true;
}

bool sampleWeightChannel(const cgltf_animation_channel &channel, float time, cgltf_size weightIndex,
	cgltf_size weightCount, float &value)
{
	const cgltf_animation_sampler &sampler = *channel.sampler;
	const cgltf_accessor *times = sampler.input;
	const cgltf_accessor *output = sampler.output;
	if(!times || !output || times->count == 0 || weightIndex >= weightCount) return false;
	float firstTime = 0, lastTime = 0;
	if(!readAccessorVector(times, 0, &firstTime, 1) || !readAccessorVector(times, times->count - 1, &lastTime, 1)) return false;
	cgltf_size left = 0, right = times->count - 1;
	if(time <= firstTime) right = 0;
	else if(time >= lastTime) left = right;
	else
	{
		while(right - left > 1)
		{
			const cgltf_size middle = left + (right - left) / 2; float middleTime = 0;
			if(!readAccessorVector(times, middle, &middleTime, 1)) return false;
			if(time < middleTime) right = middle; else left = middle;
		}
	}
	float leftTime = firstTime, rightTime = lastTime;
	if(!readAccessorVector(times, left, &leftTime, 1) || !readAccessorVector(times, right, &rightTime, 1)) return false;
	const float amount = right == left ? 0.0f : (time - leftTime) / std::max(rightTime - leftTime, 1.0e-20f);
	const bool cubic = sampler.interpolation == cgltf_interpolation_type_cubic_spline;
	auto outputIndex = [&](cgltf_size key, cgltf_size slot) { return cubic ? key * weightCount * 3 + slot * weightCount + weightIndex : key * weightCount + weightIndex; };
	float a = 0, b = 0;
	if(!readAccessorVector(output, outputIndex(left, 1), &a, 1) || !readAccessorVector(output, outputIndex(right, 1), &b, 1)) return false;
	if(right == left || sampler.interpolation == cgltf_interpolation_type_step) { value = a; return true; }
	if(cubic)
	{
		float outTangent = 0, inTangent = 0;
		if(!readAccessorVector(output, outputIndex(left, 2), &outTangent, 1) ||
			!readAccessorVector(output, outputIndex(right, 0), &inTangent, 1)) return false;
		const float t2 = amount*amount, t3 = t2*amount, delta = rightTime-leftTime;
		value = (2*t3-3*t2+1)*a + (t3-2*t2+amount)*delta*outTangent +
			(-2*t3+3*t2)*b + (t3-t2)*delta*inTangent;
	}
	else value = a + (b-a)*amount;
	return true;
}

} // namespace

Converter::Converter() = default;
Converter::~Converter() { if(_data) cgltf_free(_data); }

void Converter::setError(const std::string &message) { _error = message; }

bool Converter::load(const std::string &inputFile)
{
	if(_data) { cgltf_free(_data); _data = nullptr; }
	_vertices.clear(); _indices.clear(); _ranges.clear(); _exportedImages.clear(); _morphTargets.clear(); _error.clear();
	_inputFile = inputFile; _inputDirectory = directoryOf(inputFile);
	cgltf_options options = {};
	cgltf_result result = cgltf_parse_file(&options, inputFile.c_str(), &_data);
	if(result != cgltf_result_success) { setError("Could not parse glTF/GLB file (cgltf error " + std::to_string(result) + ")"); return false; }
	result = cgltf_load_buffers(&options, _data, inputFile.c_str());
	if(result != cgltf_result_success) { setError("Could not load glTF buffers (cgltf error " + std::to_string(result) + ")"); return false; }
	result = cgltf_validate(_data);
	if(result != cgltf_result_success) { setError("glTF validation failed (cgltf error " + std::to_string(result) + ")"); return false; }
	_nodeNames.resize(static_cast<size_t>(_data->nodes_count));
	std::unordered_map<std::string, unsigned> nameCounts;
	for(cgltf_size i = 0; i < _data->nodes_count; ++i)
		if(_data->nodes[i].name && _data->nodes[i].name[0]) ++nameCounts[_data->nodes[i].name];
	for(cgltf_size i = 0; i < _data->nodes_count; ++i)
	{
		const char *sourceName = _data->nodes[i].name;
		std::string name = sourceName && sourceName[0] ? sourceName : "Node";
		if(!sourceName || !sourceName[0] || nameCounts[name] > 1) name += "_" + std::to_string(i);
		if(name.size() > 255) name.resize(255);
		_nodeNames[i] = name;
	}
	_animatedNodes.clear();
	for(cgltf_size i = 0; i < _data->animations_count; ++i)
		for(cgltf_size j = 0; j < _data->animations[i].channels_count; ++j)
			if(_data->animations[i].channels[j].target_node) _animatedNodes.insert(_data->animations[i].channels[j].target_node);
	return buildGeometry();
}

std::string Converter::summary() const
{
	if(!_data) return "No glTF loaded";
	std::ostringstream out;
	out << _data->scenes_count << " scenes, " << _data->nodes_count << " nodes, "
		<< _data->meshes_count << " meshes, " << _data->materials_count << " materials, "
		<< _data->skins_count << " skins, " << _data->animations_count << " animations";
	for(cgltf_size i = 0; i < _data->animations_count; ++i)
	{
		float duration = 0.0f;
		unsigned translations = 0, rotations = 0, scales = 0, weights = 0;
		for(cgltf_size j = 0; j < _data->animations[i].samplers_count; ++j)
		{
			const cgltf_accessor *times = _data->animations[i].samplers[j].input;
			if(times && times->has_max) duration = std::max(duration, times->max[0]);
		}
		for(cgltf_size j = 0; j < _data->animations[i].channels_count; ++j)
		{
			switch(_data->animations[i].channels[j].target_path)
			{
				case cgltf_animation_path_type_translation: ++translations; break;
				case cgltf_animation_path_type_rotation: ++rotations; break;
				case cgltf_animation_path_type_scale: ++scales; break;
				case cgltf_animation_path_type_weights: ++weights; break;
				default: break;
			}
		}
		out << "\n  animation " << i << " '" << (_data->animations[i].name ? _data->animations[i].name : "unnamed")
			<< "': " << _data->animations[i].channels_count << " channels (T=" << translations << ", R=" << rotations
			<< ", S=" << scales << ", weights=" << weights << "), " << duration << " seconds";
		for(cgltf_size j = 0; j < _data->animations[i].channels_count; ++j)
		{
			const cgltf_animation_channel &channel = _data->animations[i].channels[j];
			if(channel.target_path == cgltf_animation_path_type_weights && channel.target_node)
				out << "\n    weights target '" << nodeName(channel.target_node) << "': "
					<< (channel.target_node->mesh && channel.target_node->mesh->primitives_count ?
						channel.target_node->mesh->primitives[0].targets_count : 0) << " morph weights";
		}
	}
	return out.str();
}

bool Converter::buildGeometry()
{
	for(cgltf_size meshIndex = 0; meshIndex < _data->meshes_count; ++meshIndex)
	{
		cgltf_mesh &mesh = _data->meshes[meshIndex];
		for(cgltf_size primitiveIndex = 0; primitiveIndex < mesh.primitives_count; ++primitiveIndex)
		{
			PrimitiveRange range;
			if(!appendPrimitive(mesh, mesh.primitives[primitiveIndex], range)) return false;
			_ranges[&mesh.primitives[primitiveIndex]] = range;
		}
	}
	return true;
}

bool Converter::appendPrimitive(const cgltf_mesh &mesh, const cgltf_primitive &primitive, PrimitiveRange &range)
{
	if(primitive.type != cgltf_primitive_type_triangles)
	{
		setError("Only triangle-list glTF primitives are currently supported"); return false;
	}
	if(primitive.has_draco_mesh_compression)
	{
		setError("KHR_draco_mesh_compression is not supported; export an uncompressed GLB"); return false;
	}
	const cgltf_accessor *positions = findAttribute(primitive, cgltf_attribute_type_position);
	const cgltf_accessor *normals = findAttribute(primitive, cgltf_attribute_type_normal);
	const cgltf_accessor *tangents = findAttribute(primitive, cgltf_attribute_type_tangent);
	const cgltf_accessor *uv0 = findAttribute(primitive, cgltf_attribute_type_texcoord, 0);
	const cgltf_accessor *uv1 = findAttribute(primitive, cgltf_attribute_type_texcoord, 1);
	if(!positions || positions->count == 0 || positions->count > std::numeric_limits<uint32_t>::max())
	{
		setError("A glTF primitive has no valid POSITION accessor"); return false;
	}

	std::vector<Vertex> local(static_cast<size_t>(positions->count));
	for(cgltf_size i = 0; i < positions->count; ++i)
	{
		if(!cgltf_accessor_read_float(positions, i, local[i].position, 3)) { setError("Could not read POSITION accessor"); return false; }
		if(normals && !cgltf_accessor_read_float(normals, i, local[i].normal, 3)) { setError("Could not read NORMAL accessor"); return false; }
		if(uv0 && !cgltf_accessor_read_float(uv0, i, local[i].uv0, 2)) { setError("Could not read TEXCOORD_0 accessor"); return false; }
		if(uv1 && !cgltf_accessor_read_float(uv1, i, local[i].uv1, 2)) { setError("Could not read TEXCOORD_1 accessor"); return false; }
		if(tangents)
		{
			float tangent4[4];
			if(!cgltf_accessor_read_float(tangents, i, tangent4, 4)) { setError("Could not read TANGENT accessor"); return false; }
			std::copy(tangent4, tangent4 + 3, local[i].tangent);
			cross3(local[i].normal, local[i].tangent, local[i].bitangent);
			for(float &component : local[i].bitangent) component *= tangent4[3];
		}
	}

	const cgltf_size indexCount = primitive.indices ? primitive.indices->count : positions->count;
	if(indexCount % 3 != 0 || indexCount > std::numeric_limits<uint32_t>::max())
	{
		setError("A glTF triangle primitive has an invalid index count"); return false;
	}
	std::vector<uint32_t> localIndices(static_cast<size_t>(indexCount));
	for(cgltf_size i = 0; i < indexCount; ++i)
	{
		const cgltf_size index = primitive.indices ? cgltf_accessor_read_index(primitive.indices, i) : i;
		if(index >= positions->count) { setError("A glTF index is outside its vertex accessor"); return false; }
		localIndices[i] = static_cast<uint32_t>(index);
	}
	calculateBasis(local, localIndices, normals != nullptr, tangents != nullptr);

	if(_vertices.size() + local.size() > std::numeric_limits<uint32_t>::max())
	{
		setError("Converted geometry exceeds Horde3D's 32-bit vertex limit"); return false;
	}
	const uint32_t vertexBase = static_cast<uint32_t>(_vertices.size());
	range.first = static_cast<uint32_t>(_indices.size());
	range.count = static_cast<uint32_t>(localIndices.size());
	range.vertexStart = vertexBase;
	range.vertexEnd = vertexBase + static_cast<uint32_t>(local.size()) - 1;
	range.material = materialName(primitive.material);
	_vertices.insert(_vertices.end(), local.begin(), local.end());
	for(uint32_t index : localIndices) _indices.push_back(vertexBase + index);

	for(cgltf_size targetIndex = 0; targetIndex < primitive.targets_count; ++targetIndex)
	{
		const std::string name = morphName(&mesh, static_cast<size_t>(targetIndex));
		auto existing = std::find_if(_morphTargets.begin(), _morphTargets.end(), [&](const MorphTarget &target) { return target.name == name; });
		if(existing == _morphTargets.end()) { _morphTargets.push_back(MorphTarget{}); existing = _morphTargets.end() - 1; existing->name = name; }
		const cgltf_morph_target &sourceTarget = primitive.targets[targetIndex];
		const cgltf_accessor *positionsDelta = findAttribute(sourceTarget, cgltf_attribute_type_position);
		const cgltf_accessor *normalsDelta = findAttribute(sourceTarget, cgltf_attribute_type_normal);
		const cgltf_accessor *tangentsDelta = findAttribute(sourceTarget, cgltf_attribute_type_tangent);
		for(cgltf_size vertexIndex = 0; vertexIndex < positions->count; ++vertexIndex)
		{
			MorphDiff diff; diff.vertex = vertexBase + static_cast<uint32_t>(vertexIndex);
			if(positionsDelta && !cgltf_accessor_read_float(positionsDelta, vertexIndex, diff.position, 3)) { setError("Could not read morph POSITION accessor"); return false; }
			if(normalsDelta && !cgltf_accessor_read_float(normalsDelta, vertexIndex, diff.normal, 3)) { setError("Could not read morph NORMAL accessor"); return false; }
			if(tangentsDelta && !cgltf_accessor_read_float(tangentsDelta, vertexIndex, diff.tangent, 3)) { setError("Could not read morph TANGENT accessor"); return false; }
			const Vertex &base = local[vertexIndex];
			float targetNormal[3], targetTangent[3], targetBitangent[3];
			for(int axis = 0; axis < 3; ++axis) { targetNormal[axis] = base.normal[axis] + diff.normal[axis]; targetTangent[axis] = base.tangent[axis] + diff.tangent[axis]; }
			normalize3(targetNormal); normalize3(targetTangent); cross3(targetNormal, targetTangent, targetBitangent);
			if(dot3(targetBitangent, base.bitangent) < 0.0f) for(float &component : targetBitangent) component = -component;
			for(int axis = 0; axis < 3; ++axis) diff.bitangent[axis] = targetBitangent[axis] - base.bitangent[axis];
			existing->diffs.push_back(diff);
		}
	}
	return true;
}

std::string Converter::materialName(const cgltf_material *material) const
{
	if(!material) return "Default";
	const size_t index = static_cast<size_t>(material - _data->materials);
	std::vector<std::string> assigned;
	assigned.reserve(index + 1);
	for(size_t i = 0; i <= index; ++i)
	{
		const cgltf_material &current = _data->materials[i];
		const std::string base = sanitize(current.name, "Material_" + std::to_string(i));
		std::string candidate = base;
		if(std::find(assigned.begin(), assigned.end(), candidate) != assigned.end())
		{
			candidate += "_" + std::to_string(i);
			while(std::find(assigned.begin(), assigned.end(), candidate) != assigned.end()) candidate += "_";
		}
		assigned.push_back(candidate);
	}
	return assigned.back();
}

std::string Converter::nodeName(const cgltf_node *node) const
{
	if(!node || node < _data->nodes || node >= _data->nodes + _data->nodes_count) return "Node";
	return _nodeNames[static_cast<size_t>(node - _data->nodes)];
}

std::string Converter::morphName(const cgltf_mesh *mesh, size_t targetIndex) const
{
	const size_t meshIndex = mesh && mesh >= _data->meshes && mesh < _data->meshes + _data->meshes_count ?
		static_cast<size_t>(mesh - _data->meshes) : 0;
	const std::string meshPart = sanitize(mesh ? mesh->name : nullptr, "Mesh_" + std::to_string(meshIndex));
	const char *target = mesh && targetIndex < mesh->target_names_count ? mesh->target_names[targetIndex] : nullptr;
	return meshPart + "_" + sanitize(target, "Target_" + std::to_string(targetIndex));
}

bool Converter::write(const std::string &destination, const std::string &assetPath)
{
	if(!_data) { setError("No glTF file has been loaded"); return false; }
	_destination = destination.empty() ? "." : destination;
	std::string cleanAsset = assetPath;
	std::replace(cleanAsset.begin(), cleanAsset.end(), '\\', '/');
	while(!cleanAsset.empty() && cleanAsset.front() == '/') cleanAsset.erase(cleanAsset.begin());
	while(!cleanAsset.empty() && cleanAsset.back() == '/') cleanAsset.pop_back();
	if(cleanAsset.find("..") != std::string::npos) { setError("Asset path must stay inside the destination"); return false; }
	if(!makeDirectory(joinPath(_destination, cleanAsset))) { setError("Could not create output directory"); return false; }
	const std::string baseName = sanitize(fileStem(_inputFile).c_str(), "model");
	return writeGeometry(joinPath(cleanAsset, baseName)) && writeMaterials(baseName, cleanAsset) &&
		writeScene(baseName, cleanAsset) && writeAnimations(baseName, cleanAsset);
}

bool Converter::writeGeometry(const std::string &relativeBase)
{
	const std::string path = joinPath(_destination, relativeBase + ".geo");
	FILE *file = std::fopen(path.c_str(), "wb");
	if(!file) { setError("Could not write " + path); return false; }
	bool ok = true;
	const uint32_t version = 5, oneJoint = 1, streamCount = 6, vertexCount = static_cast<uint32_t>(_vertices.size());
	ok &= writeLE(file, "H3DG", 4); ok &= writeLE(file, &version, 1); ok &= writeLE(file, &oneJoint, 1);
	const float identity[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
	ok &= writeLE(file, identity, 16); ok &= writeLE(file, &streamCount, 1); ok &= writeLE(file, &vertexCount, 1);
	for(uint32_t stream = 0; stream < 8; ++stream)
	{
		if(stream == 4 || stream == 5) continue;
		const uint32_t elementSize = stream == 0 ? 12u : (stream < 4 ? 6u : 8u);
		ok &= writeLE(file, &stream, 1); ok &= writeLE(file, &elementSize, 1);
		for(const Vertex &vertex : _vertices)
		{
			if(stream == 0) ok &= writeLE(file, vertex.position, 3);
			else if(stream >= 1 && stream <= 3)
			{
				const float *source = stream == 1 ? vertex.normal : (stream == 2 ? vertex.tangent : vertex.bitangent);
				const short packed[3] = { packedNormal(source[0]), packedNormal(source[1]), packedNormal(source[2]) };
				ok &= writeLE(file, packed, 3);
			}
			else ok &= writeLE(file, stream == 6 ? vertex.uv0 : vertex.uv1, 2);
		}
	}
	const uint32_t indexCount = static_cast<uint32_t>(_indices.size());
	ok &= writeLE(file, &indexCount, 1); if(indexCount) ok &= writeLE(file, _indices.data(), _indices.size());
	const uint32_t morphCount = static_cast<uint32_t>(_morphTargets.size());
	ok &= writeLE(file, &morphCount, 1);
	for(const MorphTarget &target : _morphTargets)
	{
		char fixedName[256] = {};
		std::memcpy(fixedName, target.name.data(), std::min(target.name.size(), sizeof(fixedName) - 1));
		ok &= writeLE(file, fixedName, sizeof(fixedName));
		const uint32_t diffCount = static_cast<uint32_t>(target.diffs.size()), morphStreams = 4;
		ok &= writeLE(file, &diffCount, 1);
		for(const MorphDiff &diff : target.diffs) ok &= writeLE(file, &diff.vertex, 1);
		ok &= writeLE(file, &morphStreams, 1);
		for(uint32_t stream = 0; stream < morphStreams; ++stream)
		{
			const uint32_t elementSize = 3 * sizeof(float);
			ok &= writeLE(file, &stream, 1); ok &= writeLE(file, &elementSize, 1);
			for(const MorphDiff &diff : target.diffs)
			{
				const float *values = stream == 0 ? diff.position : (stream == 1 ? diff.normal : (stream == 2 ? diff.tangent : diff.bitangent));
				ok &= writeLE(file, values, 3);
			}
		}
	}
	std::fclose(file);
	if(!ok) setError("Failed while writing " + path);
	return ok;
}

bool Converter::writeNode(const cgltf_node &node, const std::string &baseName,
	const std::string &assetPath, unsigned depth, std::string &xml)
{
	const std::string indent(depth + 1, '\t');
	const std::string name = xmlEscape(nodeName(&node));
	const std::string transform = transformAttributes(node);
	const bool hasMesh = node.mesh && node.mesh->primitives_count > 0;
	if(!hasMesh)
	{
		// Horde Model hierarchies only allow Mesh/Joint parents for Mesh children.
		// Identity-index joints preserve arbitrary glTF transform nodes and remain animatable.
		const std::string tag = "Joint";
		xml += indent + "<" + tag + " name=\"" + name + "\"" + transform;
		xml += " jointIndex=\"0\"";
		if(node.children_count == 0) { xml += " />\n"; return true; }
		xml += ">\n";
		for(cgltf_size i = 0; i < node.children_count; ++i)
			if(!writeNode(*node.children[i], baseName, assetPath, depth + 1, xml)) return false;
		xml += indent + "</" + tag + ">\n";
		return true;
	}

	const cgltf_primitive &firstPrimitive = node.mesh->primitives[0];
	auto first = _ranges.find(&firstPrimitive);
	if(first == _ranges.end()) { setError("Internal error: missing primitive range"); return false; }
	auto meshTag = [&](const std::string &meshName, const PrimitiveRange &range, const std::string &extraTransform) {
		std::ostringstream out;
		out << "<Mesh name=\"" << xmlEscape(meshName) << "\"" << extraTransform
			<< " material=\"" << xmlEscape(joinPath(assetPath, baseName + "_" + range.material + ".material.xml")) << "\""
			<< " batchStart=\"" << range.first << "\" batchCount=\"" << range.count
			<< "\" vertRStart=\"" << range.vertexStart << "\" vertREnd=\"" << range.vertexEnd << "\"";
		return out.str();
	};
	const bool hasContents = node.mesh->primitives_count > 1 || node.children_count > 0;
	xml += indent + meshTag(name, first->second, transform);
	if(!hasContents) { xml += " />\n"; return true; }
	xml += ">\n";
	for(cgltf_size i = 1; i < node.mesh->primitives_count; ++i)
	{
		auto range = _ranges.find(&node.mesh->primitives[i]);
		if(range == _ranges.end()) { setError("Internal error: missing primitive range"); return false; }
		xml += indent + "\t" + meshTag("#" + name + "_" + std::to_string(i), range->second, "") + " />\n";
	}
	for(cgltf_size i = 0; i < node.children_count; ++i)
		if(!writeNode(*node.children[i], baseName, assetPath, depth + 1, xml)) return false;
	xml += indent + "</Mesh>\n";
	return true;
}

bool Converter::writeScene(const std::string &baseName, const std::string &assetPath)
{
	std::string xml = "<Model name=\"" + xmlEscape(baseName) + "\" geometry=\"" +
		xmlEscape(joinPath(assetPath, baseName + ".geo")) + "\">\n";
	const cgltf_scene *scene = _data->scene ? _data->scene : (_data->scenes_count ? &_data->scenes[0] : nullptr);
	if(scene)
	{
		for(cgltf_size i = 0; i < scene->nodes_count; ++i)
			if(!writeNode(*scene->nodes[i], baseName, assetPath, 0, xml)) return false;
	}
	else
	{
		for(cgltf_size i = 0; i < _data->nodes_count; ++i)
			if(_data->nodes[i].parent == nullptr && !writeNode(_data->nodes[i], baseName, assetPath, 0, xml)) return false;
	}
	xml += "</Model>\n";
	const std::string path = joinPath(joinPath(_destination, assetPath), baseName + ".scene.xml");
	std::ofstream output(path);
	output << xml;
	if(!output.good()) { setError("Could not write " + path); return false; }
	return true;
}

std::string Converter::exportImage(const cgltf_image *image, const std::string &assetPath,
	const std::string &fallbackName)
{
	if(!image) return std::string();
	auto existing = _exportedImages.find(image);
	if(existing != _exportedImages.end()) return existing->second;
	const size_t imageIndex = static_cast<size_t>(image - _data->images);
	std::string outputName;
	std::vector<unsigned char> embedded;
	const bool convertWebP = isWebP(*image);
	if(image->uri && std::strncmp(image->uri, "data:", 5) != 0)
	{
		outputName = sanitize(fileStem(fileName(image->uri)).c_str(), fallbackName) + (convertWebP ? ".tga" : imageExtension(*image));
		const std::string sourcePath = joinPath(_inputDirectory, image->uri);
		const std::string destinationPath = joinPath(joinPath(_destination, assetPath), outputName);
		const bool copied = convertWebP ? (readFile(sourcePath, embedded) && writeWebPAsTga(embedded, destinationPath))
			: copyFile(sourcePath, destinationPath);
		if(!copied)
		{
			setError("Could not copy glTF image " + std::string(image->uri)); return std::string();
		}
	}
	else
	{
		outputName = sanitize(image->name, fallbackName + "_" + std::to_string(imageIndex)) + (convertWebP ? ".tga" : imageExtension(*image));
		if(image->buffer_view)
		{
			const unsigned char *begin = static_cast<const unsigned char *>(image->buffer_view->buffer->data) + image->buffer_view->offset;
			embedded.assign(begin, begin + image->buffer_view->size);
		}
		else if(image->uri)
		{
			const char *comma = std::strchr(image->uri, ',');
			if(!comma || std::strstr(image->uri, ";base64") == nullptr) { setError("Only base64 embedded image URIs are supported"); return std::string(); }
			embedded = decodeBase64(comma + 1);
		}
		if(embedded.empty()) { setError("Embedded glTF image contains no data"); return std::string(); }
		const std::string destinationPath = joinPath(joinPath(_destination, assetPath), outputName);
		if(convertWebP)
		{
			if(!writeWebPAsTga(embedded, destinationPath)) { setError("Could not decode embedded WebP image"); return std::string(); }
		}
		else
		{
			std::ofstream output(destinationPath, std::ios::binary);
			output.write(reinterpret_cast<const char *>(embedded.data()), static_cast<std::streamsize>(embedded.size()));
			if(!output.good()) { setError("Could not export embedded glTF image"); return std::string(); }
		}
	}
	const std::string resource = joinPath(assetPath, outputName);
	_exportedImages[image] = resource;
	return resource;
}

bool Converter::writeMaterials(const std::string &baseName, const std::string &assetPath)
{
	const cgltf_material *materials = _data->materials;
	const cgltf_size materialCount = _data->materials_count;
	for(cgltf_size i = 0; i <= materialCount; ++i)
	{
		const bool isDefault = i == materialCount;
		const cgltf_material *material = isDefault ? nullptr : &materials[i];
		const cgltf_pbr_metallic_roughness *pbr = material && material->has_pbr_metallic_roughness ? &material->pbr_metallic_roughness : nullptr;
		const cgltf_iridescence *iridescence = material && material->has_iridescence ? &material->iridescence : nullptr;
		const cgltf_transmission *transmission = material && material->has_transmission ? &material->transmission : nullptr;
		const cgltf_volume *volume = material && material->has_volume ? &material->volume : nullptr;
		const cgltf_clearcoat *clearcoat = material && material->has_clearcoat ? &material->clearcoat : nullptr;
		const cgltf_anisotropy *anisotropy = material && material->has_anisotropy ? &material->anisotropy : nullptr;
		const float defaultColor[4] = { 1,1,1,1 };
		const float *color = pbr ? pbr->base_color_factor : defaultColor;
		const float metallic = pbr ? pbr->metallic_factor : 1.0f;
		const float roughness = pbr ? pbr->roughness_factor : 1.0f;
		const float defaultEmissive[3] = { 0,0,0 };
		const float *emissive = material ? material->emissive_factor : defaultEmissive;
		const float emissiveStrength = material && material->has_emissive_strength
			? material->emissive_strength.emissive_strength : 1.0f;
		const cgltf_image *baseImage = pbr ? textureImage(pbr->base_color_texture) : nullptr;
		const cgltf_image *metalImage = pbr ? textureImage(pbr->metallic_roughness_texture) : nullptr;
		const cgltf_image *normalImage = material ? textureImage(material->normal_texture) : nullptr;
		const cgltf_image *occlusionImage = material ? textureImage(material->occlusion_texture) : nullptr;
		const cgltf_image *emissiveImage = material ? textureImage(material->emissive_texture) : nullptr;
		const cgltf_image *iridescenceImage = iridescence ? textureImage(iridescence->iridescence_texture) : nullptr;
		const cgltf_image *iridescenceThicknessImage = iridescence ? textureImage(iridescence->iridescence_thickness_texture) : nullptr;
		const cgltf_image *transmissionImage = transmission ? textureImage(transmission->transmission_texture) : nullptr;
		const cgltf_image *volumeThicknessImage = volume ? textureImage(volume->thickness_texture) : nullptr;
		auto map = [&](const cgltf_image *image, const std::string &fallback, const char *defaultMap) {
			if(!image) return std::string(defaultMap);
			return exportImage(image, assetPath, baseName + "_" + fallback);
		};
		const std::string baseMap = map(baseImage, "baseColor", "textures/common/white.tga"); if(baseMap.empty()) return false;
		const std::string metalMap = map(metalImage, "metalRough", "textures/common/white.tga"); if(metalMap.empty()) return false;
		const std::string normalMap = map(normalImage, "normal", "textures/common/defnorm.tga"); if(normalMap.empty()) return false;
		const std::string occlusionMap = map(occlusionImage, "occlusion", "textures/common/white.tga"); if(occlusionMap.empty()) return false;
		const std::string emissiveMap = map(emissiveImage, "emissive", "textures/common/white.tga"); if(emissiveMap.empty()) return false;
		const std::string iridescenceMap = map(iridescenceImage, "iridescence", "textures/common/white.tga"); if(iridescenceMap.empty()) return false;
		const std::string iridescenceThicknessMap = map(iridescenceThicknessImage, "iridescenceThickness", "textures/common/white.tga"); if(iridescenceThicknessMap.empty()) return false;
		const std::string transmissionMap = map(transmissionImage, "transmission", "textures/common/white.tga"); if(transmissionMap.empty()) return false;
		const std::string volumeThicknessMap = map(volumeThicknessImage, "volumeThickness", "textures/common/white.tga"); if(volumeThicknessMap.empty()) return false;
		const TextureTransform baseTransform = textureTransform(pbr ? &pbr->base_color_texture : nullptr);
		const TextureTransform metalTransform = textureTransform(pbr ? &pbr->metallic_roughness_texture : nullptr);
		const TextureTransform normalTransform = textureTransform(material ? &material->normal_texture : nullptr);
		const TextureTransform occlusionTransform = textureTransform(material ? &material->occlusion_texture : nullptr);
		const TextureTransform emissiveTransform = textureTransform(material ? &material->emissive_texture : nullptr);
		const TextureTransform iridescenceTransform = textureTransform(iridescence ? &iridescence->iridescence_texture : nullptr);
		const TextureTransform iridescenceThicknessTransform = textureTransform(iridescence ? &iridescence->iridescence_thickness_texture : nullptr);
		const TextureTransform transmissionTransform = textureTransform(transmission ? &transmission->transmission_texture : nullptr);
		const TextureTransform volumeThicknessTransform = textureTransform(volume ? &volume->thickness_texture : nullptr);
		const float materialIor = material && material->has_ior ? material->ior.ior : 1.5f;

		const std::string name = isDefault ? "Default" : materialName(material);
		const std::string path = joinPath(joinPath(_destination, assetPath), baseName + "_" + name + ".material.xml");
		std::ofstream out(path);
		const bool isTransmissive = transmission && transmission->transmission_factor > 0.0f;
		out << std::setprecision(9) << "<Material" << (isTransmissive ? " class=\"Translucent\"" : "")
			<< ">\n\t<Shader source=\"shaders/gltf_pbr.shader\" />\n"
			<< "\t<Sampler name=\"baseColorMap\" map=\"" << xmlEscape(baseMap) << "\" />\n"
			<< "\t<Sampler name=\"metallicRoughnessMap\" map=\"" << xmlEscape(metalMap) << "\" />\n"
			<< "\t<Sampler name=\"normalMap\" map=\"" << xmlEscape(normalMap) << "\" allowCompression=\"false\" />\n"
			<< "\t<Sampler name=\"occlusionMap\" map=\"" << xmlEscape(occlusionMap) << "\" />\n"
			<< "\t<Sampler name=\"emissiveMap\" map=\"" << xmlEscape(emissiveMap) << "\" />\n"
			<< "\t<Sampler name=\"iridescenceMap\" map=\"" << xmlEscape(iridescenceMap) << "\" />\n"
			<< "\t<Sampler name=\"iridescenceThicknessMap\" map=\"" << xmlEscape(iridescenceThicknessMap) << "\" />\n"
			<< "\t<Sampler name=\"transmissionMap\" map=\"" << xmlEscape(transmissionMap) << "\" />\n"
			<< "\t<Sampler name=\"volumeThicknessMap\" map=\"" << xmlEscape(volumeThicknessMap) << "\" />\n"
			<< "\t<Sampler name=\"iblIrradiance\" map=\"models/skybox/skybox.dds\" />\n"
			<< "\t<Sampler name=\"iblSpecular0\" map=\"models/skybox/skybox.dds\" />\n"
			<< "\t<Sampler name=\"iblSpecular1\" map=\"models/skybox/skybox.dds\" />\n"
			<< "\t<Sampler name=\"iblSpecular2\" map=\"models/skybox/skybox.dds\" />\n"
			<< "\t<Sampler name=\"iblSpecular3\" map=\"models/skybox/skybox.dds\" />\n"
			<< "\t<Sampler name=\"iblSpecular4\" map=\"models/skybox/skybox.dds\" />\n"
			<< "\t<Sampler name=\"iblBrdfLut\" map=\"textures/common/white.tga\" />\n"
			<< "\t<Uniform name=\"baseColorFactor\" a=\"" << color[0] << "\" b=\"" << color[1] << "\" c=\"" << color[2] << "\" d=\"" << color[3] << "\" />\n"
			<< "\t<Uniform name=\"pbrParams\" a=\"" << metallic << "\" b=\"" << roughness << "\" c=\""
			<< (material ? material->occlusion_texture.scale : 1.0f) << "\" d=\"1\" />\n"
			<< "\t<Uniform name=\"emissiveFactor\" a=\"" << emissive[0] << "\" b=\"" << emissive[1] << "\" c=\"" << emissive[2]
			<< "\" d=\"" << emissiveStrength << "\" />\n"
			<< "\t<Uniform name=\"normalScale\" a=\"" << (material ? material->normal_texture.scale : 1.0f) << "\" />\n"
			<< "\t<Uniform name=\"baseIor\" a=\"" << materialIor << "\" />\n"
			<< "\t<Uniform name=\"iridescenceParams\" a=\"" << (iridescence ? iridescence->iridescence_factor : 0.0f)
			<< "\" b=\"" << (iridescence ? iridescence->iridescence_ior : 1.3f)
			<< "\" c=\"" << (iridescence ? iridescence->iridescence_thickness_min : 100.0f)
			<< "\" d=\"" << (iridescence ? iridescence->iridescence_thickness_max : 400.0f) << "\" />\n"
			<< "\t<Uniform name=\"transmissionParams\" a=\"" << (transmission ? transmission->transmission_factor : 0.0f)
			<< "\" b=\"" << (volume ? volume->thickness_factor : 0.0f)
			<< "\" c=\"" << (volume ? volume->attenuation_distance : 1.0e20f) << "\" />\n"
			<< "\t<Uniform name=\"clearcoatParams\" a=\"" << (clearcoat ? clearcoat->clearcoat_factor : 0.0f)
			<< "\" b=\"" << (clearcoat ? clearcoat->clearcoat_roughness_factor : 0.0f)
			<< "\" c=\"" << (clearcoat && clearcoat->clearcoat_normal_texture.texture
				? clearcoat->clearcoat_normal_texture.scale : 1.0f) << "\" d=\"0\" />\n"
			<< "\t<Uniform name=\"anisotropyParams\" a=\"" << (anisotropy ? anisotropy->anisotropy_strength : 0.0f)
			<< "\" b=\"" << (anisotropy ? anisotropy->anisotropy_rotation : 0.0f) << "\" c=\"0\" d=\"0\" />\n"
			<< "\t<Uniform name=\"attenuationColor\" a=\"" << (volume ? volume->attenuation_color[0] : 1.0f)
			<< "\" b=\"" << (volume ? volume->attenuation_color[1] : 1.0f)
			<< "\" c=\"" << (volume ? volume->attenuation_color[2] : 1.0f) << "\" d=\"1\" />\n"
			<< "\t<Uniform name=\"baseColorTexTransform\" a=\"" << baseTransform.offset[0] << "\" b=\"" << baseTransform.offset[1]
			<< "\" c=\"" << baseTransform.scale[0] << "\" d=\"" << baseTransform.scale[1] << "\" />\n"
			<< "\t<Uniform name=\"metalRoughTexTransform\" a=\"" << metalTransform.offset[0] << "\" b=\"" << metalTransform.offset[1]
			<< "\" c=\"" << metalTransform.scale[0] << "\" d=\"" << metalTransform.scale[1] << "\" />\n"
			<< "\t<Uniform name=\"normalTexTransform\" a=\"" << normalTransform.offset[0] << "\" b=\"" << normalTransform.offset[1]
			<< "\" c=\"" << normalTransform.scale[0] << "\" d=\"" << normalTransform.scale[1] << "\" />\n"
			<< "\t<Uniform name=\"occlusionTexTransform\" a=\"" << occlusionTransform.offset[0] << "\" b=\"" << occlusionTransform.offset[1]
			<< "\" c=\"" << occlusionTransform.scale[0] << "\" d=\"" << occlusionTransform.scale[1] << "\" />\n"
			<< "\t<Uniform name=\"emissiveTexTransform\" a=\"" << emissiveTransform.offset[0] << "\" b=\"" << emissiveTransform.offset[1]
			<< "\" c=\"" << emissiveTransform.scale[0] << "\" d=\"" << emissiveTransform.scale[1] << "\" />\n"
			<< "\t<Uniform name=\"iridescenceTexTransform\" a=\"" << iridescenceTransform.offset[0] << "\" b=\"" << iridescenceTransform.offset[1]
			<< "\" c=\"" << iridescenceTransform.scale[0] << "\" d=\"" << iridescenceTransform.scale[1] << "\" />\n"
			<< "\t<Uniform name=\"iridescenceThicknessTexTransform\" a=\"" << iridescenceThicknessTransform.offset[0] << "\" b=\"" << iridescenceThicknessTransform.offset[1]
			<< "\" c=\"" << iridescenceThicknessTransform.scale[0] << "\" d=\"" << iridescenceThicknessTransform.scale[1] << "\" />\n"
			<< "\t<Uniform name=\"transmissionTexTransform\" a=\"" << transmissionTransform.offset[0] << "\" b=\"" << transmissionTransform.offset[1]
			<< "\" c=\"" << transmissionTransform.scale[0] << "\" d=\"" << transmissionTransform.scale[1] << "\" />\n"
			<< "\t<Uniform name=\"volumeThicknessTexTransform\" a=\"" << volumeThicknessTransform.offset[0] << "\" b=\"" << volumeThicknessTransform.offset[1]
			<< "\" c=\"" << volumeThicknessTransform.scale[0] << "\" d=\"" << volumeThicknessTransform.scale[1] << "\" />\n"
			<< "\t<Uniform name=\"baseColorTexRotation\" a=\"" << baseTransform.rotation << "\" />\n"
			<< "\t<Uniform name=\"metalRoughTexRotation\" a=\"" << metalTransform.rotation << "\" />\n"
			<< "\t<Uniform name=\"normalTexRotation\" a=\"" << normalTransform.rotation << "\" />\n"
			<< "\t<Uniform name=\"occlusionTexRotation\" a=\"" << occlusionTransform.rotation << "\" />\n"
			<< "\t<Uniform name=\"emissiveTexRotation\" a=\"" << emissiveTransform.rotation << "\" />\n"
			<< "\t<Uniform name=\"iridescenceTexRotation\" a=\"" << iridescenceTransform.rotation << "\" />\n"
			<< "\t<Uniform name=\"iridescenceThicknessTexRotation\" a=\"" << iridescenceThicknessTransform.rotation << "\" />\n"
			<< "\t<Uniform name=\"transmissionTexRotation\" a=\"" << transmissionTransform.rotation << "\" />\n"
			<< "\t<Uniform name=\"volumeThicknessTexRotation\" a=\"" << volumeThicknessTransform.rotation << "\" />\n"
			<< "\t<Uniform name=\"pbrDebugView\" a=\"0\" />\n"
			<< "</Material>\n";
		if(!out.good()) { setError("Could not write " + path); return false; }
	}
	return true;
}

bool Converter::writeAnimations(const std::string &baseName, const std::string &assetPath, float framesPerSecond)
{
	for(cgltf_size animationIndex = 0; animationIndex < _data->animations_count; ++animationIndex)
	{
		const cgltf_animation &animation = _data->animations[animationIndex];
		float startTime = std::numeric_limits<float>::max(), endTime = 0.0f;
		std::vector<const cgltf_node *> nodes;
		std::unordered_set<const cgltf_node *> seen;
		for(cgltf_size i = 0; i < animation.channels_count; ++i)
		{
			const cgltf_animation_channel &channel = animation.channels[i];
			if(channel.target_node && seen.insert(channel.target_node).second) nodes.push_back(channel.target_node);
			const cgltf_accessor *times = channel.sampler ? channel.sampler->input : nullptr;
			if(times && times->count)
			{
				float first = 0, last = 0;
				if(!readAccessorVector(times, 0, &first, 1) || !readAccessorVector(times, times->count - 1, &last, 1))
				{ setError("Could not read glTF animation times"); return false; }
				startTime = std::min(startTime, first); endTime = std::max(endTime, last);
			}
		}
		if(nodes.empty()) continue;
		if(startTime == std::numeric_limits<float>::max()) startTime = 0.0f;
		const uint32_t frameCount = std::max(1u, static_cast<uint32_t>(std::ceil((endTime - startTime) * framesPerSecond)) + 1u);
		std::vector<std::vector<AnimationFrame>> nodeFrames(nodes.size(), std::vector<AnimationFrame>(frameCount));
		struct OutputMorphTrack { std::string name; std::vector<float> weights; };
		std::vector<std::vector<OutputMorphTrack>> nodeMorphTracks(nodes.size());
		for(size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex)
		{
			const AnimationFrame base = defaultFrame(*nodes[nodeIndex]);
			std::fill(nodeFrames[nodeIndex].begin(), nodeFrames[nodeIndex].end(), base);
		}
		for(cgltf_size channelIndex = 0; channelIndex < animation.channels_count; ++channelIndex)
		{
			const cgltf_animation_channel &channel = animation.channels[channelIndex];
			auto target = std::find(nodes.begin(), nodes.end(), channel.target_node);
			if(target == nodes.end()) continue;
			const size_t nodeIndex = static_cast<size_t>(target - nodes.begin());
			if(channel.target_path == cgltf_animation_path_type_weights)
			{
				const cgltf_mesh *mesh = channel.target_node ? channel.target_node->mesh : nullptr;
				const cgltf_size morphCount = mesh && mesh->primitives_count ? mesh->primitives[0].targets_count : 0;
				if(morphCount == 0) { setError("Morph animation targets a node without morph targets"); return false; }
				nodeMorphTracks[nodeIndex].resize(static_cast<size_t>(morphCount));
				for(cgltf_size morphIndex = 0; morphIndex < morphCount; ++morphIndex)
				{
					OutputMorphTrack &track = nodeMorphTracks[nodeIndex][morphIndex];
					track.name = morphName(mesh, static_cast<size_t>(morphIndex));
					track.weights.resize(frameCount);
					for(uint32_t frameIndex = 0; frameIndex < frameCount; ++frameIndex)
					{
						const float time = std::min(endTime, startTime + static_cast<float>(frameIndex) / framesPerSecond);
						if(!sampleWeightChannel(channel, time, morphIndex, morphCount, track.weights[frameIndex]))
						{ setError("Could not sample glTF morph animation channel"); return false; }
					}
				}
				continue;
			}
			for(uint32_t frameIndex = 0; frameIndex < frameCount; ++frameIndex)
			{
				const float time = std::min(endTime, startTime + static_cast<float>(frameIndex) / framesPerSecond);
				AnimationFrame &frame = nodeFrames[nodeIndex][frameIndex];
				float *destination = nullptr; cgltf_size components = 0;
				switch(channel.target_path)
				{
					case cgltf_animation_path_type_translation: destination = frame.translation; components = 3; break;
					case cgltf_animation_path_type_rotation: destination = frame.rotation; components = 4; break;
					case cgltf_animation_path_type_scale: destination = frame.scale; components = 3; break;
					default: setError("Unsupported glTF animation channel"); return false;
				}
				if(!sampleChannel(channel, time, destination, components)) { setError("Could not sample glTF animation channel"); return false; }
			}
		}

		std::string clipName = baseName;
		if(_data->animations_count > 1) clipName += "_" + sanitize(animation.name, "Animation_" + std::to_string(animationIndex));
		const std::string path = joinPath(joinPath(_destination, assetPath), clipName + ".anim");
		FILE *file = std::fopen(path.c_str(), "wb");
		if(!file) { setError("Could not write " + path); return false; }
		const uint32_t version = 4, entityCount = static_cast<uint32_t>(nodes.size());
		bool ok = writeLE(file, "H3DA", 4) && writeLE(file, &version, 1) &&
			writeLE(file, &entityCount, 1) && writeLE(file, &frameCount, 1);
		for(size_t nodeIndex = 0; ok && nodeIndex < nodes.size(); ++nodeIndex)
		{
			char fixedName[256] = {};
			const std::string name = nodeName(nodes[nodeIndex]);
			std::memcpy(fixedName, name.data(), std::min(name.size(), sizeof(fixedName) - 1));
			ok &= writeLE(file, fixedName, sizeof(fixedName));
			bool same = true;
			for(uint32_t frameIndex = 1; frameIndex < frameCount && same; ++frameIndex)
				same = std::memcmp(&nodeFrames[nodeIndex][0], &nodeFrames[nodeIndex][frameIndex], sizeof(AnimationFrame)) == 0;
			const unsigned char compressed = same ? 1 : 0;
			ok &= writeLE(file, &compressed, 1);
			const uint32_t framesToWrite = compressed ? 1 : frameCount;
			for(uint32_t frameIndex = 0; ok && frameIndex < framesToWrite; ++frameIndex)
			{
				const AnimationFrame &frame = nodeFrames[nodeIndex][frameIndex];
				ok &= writeLE(file, frame.rotation, 4);
				ok &= writeLE(file, frame.translation, 3);
				ok &= writeLE(file, frame.scale, 3);
			}
			const uint32_t morphTrackCount = static_cast<uint32_t>(nodeMorphTracks[nodeIndex].size());
			ok &= writeLE(file, &morphTrackCount, 1);
			for(const OutputMorphTrack &track : nodeMorphTracks[nodeIndex])
			{
				char fixedMorphName[256] = {};
				std::memcpy(fixedMorphName, track.name.data(), std::min(track.name.size(), sizeof(fixedMorphName) - 1));
				ok &= writeLE(file, fixedMorphName, sizeof(fixedMorphName));
				bool morphSame = true;
				for(size_t weightIndex = 1; weightIndex < track.weights.size() && morphSame; ++weightIndex)
					morphSame = track.weights[weightIndex] == track.weights[0];
				const unsigned char morphCompressed = morphSame ? 1 : 0;
				ok &= writeLE(file, &morphCompressed, 1);
				const size_t weightsToWrite = morphCompressed ? 1 : track.weights.size();
				ok &= writeLE(file, track.weights.data(), weightsToWrite);
			}
		}
		std::fclose(file);
		if(!ok) { setError("Failed while writing " + path); return false; }
	}
	return true;
}

} // namespace GLTFConverter
} // namespace Horde3D
