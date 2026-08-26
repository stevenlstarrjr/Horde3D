// Horde3D HDRI / image-based lighting extension
// SPDX-License-Identifier: EPL-1.0

#include "ibl.h"

#include "egCom.h"
#include "egModules.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace Horde3DHDRI {
namespace {

constexpr float Pi = 3.14159265358979323846f;

struct Vec3
{
	float x, y, z;
	Vec3 operator+(const Vec3 &other) const { return { x + other.x, y + other.y, z + other.z }; }
	Vec3 operator-(const Vec3 &other) const { return { x - other.x, y - other.y, z - other.z }; }
	Vec3 operator*(float value) const { return { x * value, y * value, z * value }; }
	Vec3 &operator+=(const Vec3 &other) { x += other.x; y += other.y; z += other.z; return *this; }
};

float dot(const Vec3 &a, const Vec3 &b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 cross(const Vec3 &a, const Vec3 &b)
{
	return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}
Vec3 normalize(const Vec3 &value)
{
	const float length = std::sqrt(std::max(dot(value, value), 1.0e-20f));
	return value * (1.0f / length);
}

struct FloatImage
{
	int width = 0;
	int height = 0;
	std::vector<float> rgba;
};

float radicalInverse(unsigned int bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return static_cast<float>(bits) * 2.3283064365386963e-10f;
}

void hammersley(unsigned int index, unsigned int count, float &x, float &y)
{
	x = static_cast<float>(index) / static_cast<float>(count);
	y = radicalInverse(index);
}

Vec3 faceDirection(int face, int x, int y, int size)
{
	const float u = 2.0f * (static_cast<float>(x) + 0.5f) / static_cast<float>(size) - 1.0f;
	const float v = 2.0f * (static_cast<float>(y) + 0.5f) / static_cast<float>(size) - 1.0f;
	switch(face)
	{
		case 0: return normalize({ 1.0f, -v, -u });
		case 1: return normalize({ -1.0f, -v, u });
		case 2: return normalize({ u, 1.0f, v });
		case 3: return normalize({ u, -1.0f, -v });
		case 4: return normalize({ u, -v, 1.0f });
		default: return normalize({ -u, -v, -1.0f });
	}
}

Vec3 sampleEquirect(const FloatImage &image, const Vec3 &direction)
{
	float u = std::atan2(direction.z, direction.x) / (2.0f * Pi) + 0.5f;
	const float v = 0.5f - std::asin(std::max(-1.0f, std::min(1.0f, direction.y))) / Pi;
	u -= std::floor(u);

	const float px = u * static_cast<float>(image.width) - 0.5f;
	const float py = std::max(0.0f, std::min(static_cast<float>(image.height - 1),
		v * static_cast<float>(image.height) - 0.5f));
	const int x0Unwrapped = static_cast<int>(std::floor(px));
	const int x0 = (x0Unwrapped % image.width + image.width) % image.width;
	const int x1 = (x0 + 1) % image.width;
	const int y0 = std::max(0, std::min(image.height - 1, static_cast<int>(std::floor(py))));
	const int y1 = std::min(y0 + 1, image.height - 1);
	const float tx = px - std::floor(px);
	const float ty = py - std::floor(py);

	auto pixel = [&](int x, int y) {
		const size_t offset = (static_cast<size_t>(y) * image.width + x) * 4;
		return Vec3{ image.rgba[offset], image.rgba[offset + 1], image.rgba[offset + 2] };
	};
	const Vec3 a = pixel(x0, y0) * (1.0f - tx) + pixel(x1, y0) * tx;
	const Vec3 b = pixel(x0, y1) * (1.0f - tx) + pixel(x1, y1) * tx;
	return a * (1.0f - ty) + b * ty;
}

std::vector<FloatImage> buildMipChain(const FloatImage &source)
{
	std::vector<FloatImage> levels;
	levels.push_back(source);
	while(levels.back().width > 1 || levels.back().height > 1)
	{
		const FloatImage &previous = levels.back();
		FloatImage next;
		next.width = std::max(1, previous.width / 2);
		next.height = std::max(1, previous.height / 2);
		next.rgba.resize(static_cast<size_t>(next.width) * next.height * 4);
		for(int y = 0; y < next.height; ++y)
			for(int x = 0; x < next.width; ++x)
				for(int component = 0; component < 4; ++component)
				{
					float sum = 0.0f;
					for(int oy = 0; oy < 2; ++oy)
						for(int ox = 0; ox < 2; ++ox)
						{
							const int sx = std::min(x * 2 + ox, previous.width - 1);
							const int sy = std::min(y * 2 + oy, previous.height - 1);
							sum += previous.rgba[(static_cast<size_t>(sy) * previous.width + sx) * 4 + component];
						}
					next.rgba[(static_cast<size_t>(y) * next.width + x) * 4 + component] = sum * 0.25f;
				}
		levels.push_back(std::move(next));
	}
	return levels;
}

FloatImage withLightingAdjustments(const FloatImage &source, float saturation, float intensity)
{
	FloatImage result = source;
	for(size_t offset = 0; offset < result.rgba.size(); offset += 4)
	{
		const float luminance = result.rgba[offset] * 0.2126f +
			result.rgba[offset + 1] * 0.7152f + result.rgba[offset + 2] * 0.0722f;
		result.rgba[offset] = (luminance + (result.rgba[offset] - luminance) * saturation) * intensity;
		result.rgba[offset + 1] = (luminance + (result.rgba[offset + 1] - luminance) * saturation) * intensity;
		result.rgba[offset + 2] = (luminance + (result.rgba[offset + 2] - luminance) * saturation) * intensity;
	}
	return result;
}

Vec3 sampleEquirectLod(const std::vector<FloatImage> &levels, const Vec3 &direction, float lod)
{
	lod = std::max(0.0f, std::min(lod, static_cast<float>(levels.size() - 1)));
	const int low = static_cast<int>(std::floor(lod));
	const int high = std::min(low + 1, static_cast<int>(levels.size() - 1));
	const float amount = lod - static_cast<float>(low);
	return sampleEquirect(levels[low], direction) * (1.0f - amount) +
		sampleEquirect(levels[high], direction) * amount;
}

void makeBasis(const Vec3 &normal, Vec3 &tangent, Vec3 &bitangent)
{
	const Vec3 up = std::fabs(normal.y) < 0.999f ? Vec3{ 0, 1, 0 } : Vec3{ 1, 0, 0 };
	tangent = normalize(cross(up, normal));
	bitangent = cross(normal, tangent);
}

Vec3 cosineHemisphere(float x, float y)
{
	const float radius = std::sqrt(x);
	const float phi = 2.0f * Pi * y;
	return { radius * std::cos(phi), radius * std::sin(phi), std::sqrt(std::max(0.0f, 1.0f - x)) };
}

Vec3 importanceSampleGGX(float x, float y, float roughness)
{
	const float a = std::max(roughness * roughness, 0.001f);
	const float phi = 2.0f * Pi * x;
	const float cosTheta = std::sqrt((1.0f - y) / (1.0f + (a * a - 1.0f) * y));
	const float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));
	return { std::cos(phi) * sinTheta, std::sin(phi) * sinTheta, cosTheta };
}

Vec3 toWorld(const Vec3 &local, const Vec3 &normal)
{
	Vec3 tangent, bitangent;
	makeBasis(normal, tangent, bitangent);
	return normalize(tangent * local.x + bitangent * local.y + normal * local.z);
}

std::vector<float> environmentFace(const FloatImage &source, int face, int size)
{
	std::vector<float> output(static_cast<size_t>(size) * size * 4);
	for(int y = 0; y < size; ++y)
		for(int x = 0; x < size; ++x)
		{
			const Vec3 color = sampleEquirect(source, faceDirection(face, x, y, size));
			const size_t offset = (static_cast<size_t>(y) * size + x) * 4;
			output[offset] = color.x; output[offset + 1] = color.y;
			output[offset + 2] = color.z; output[offset + 3] = 1.0f;
		}
	return output;
}

std::vector<float> irradianceFace(const FloatImage &source, int face, int size, int sampleCount)
{
	std::vector<float> output(static_cast<size_t>(size) * size * 4);
	for(int y = 0; y < size; ++y)
		for(int x = 0; x < size; ++x)
		{
			const Vec3 normal = faceDirection(face, x, y, size);
			Vec3 sum{ 0, 0, 0 };
			for(int sample = 0; sample < sampleCount; ++sample)
			{
				float xiX, xiY;
				hammersley(static_cast<unsigned int>(sample), static_cast<unsigned int>(sampleCount), xiX, xiY);
				sum += sampleEquirect(source, toWorld(cosineHemisphere(xiX, xiY), normal));
			}
			const Vec3 color = sum * (Pi / static_cast<float>(sampleCount));
			const size_t offset = (static_cast<size_t>(y) * size + x) * 4;
			output[offset] = color.x; output[offset + 1] = color.y;
			output[offset + 2] = color.z; output[offset + 3] = 1.0f;
		}
	return output;
}

std::vector<float> specularFace(const std::vector<FloatImage> &sourceMips, int face, int size, int sampleCount,
	float roughness)
{
	const FloatImage &source = sourceMips.front();
	const float texelSolidAngle = 4.0f * Pi /
		(static_cast<float>(source.width) * static_cast<float>(source.height));
	std::vector<float> output(static_cast<size_t>(size) * size * 4);
	for(int y = 0; y < size; ++y)
		for(int x = 0; x < size; ++x)
		{
			const Vec3 normal = faceDirection(face, x, y, size);
			const Vec3 view = normal;
			Vec3 sum{ 0, 0, 0 };
			float weight = 0.0f;
			for(int sample = 0; sample < sampleCount; ++sample)
			{
				float xiX, xiY;
				hammersley(static_cast<unsigned int>(sample), static_cast<unsigned int>(sampleCount), xiX, xiY);
				const Vec3 halfVector = toWorld(importanceSampleGGX(xiX, xiY, roughness), normal);
				const Vec3 light = normalize(halfVector * (2.0f * dot(view, halfVector)) - view);
				const float nDotL = std::max(dot(normal, light), 0.0f);
				if(nDotL > 0.0f)
				{
					const float nDotH = std::max(dot(normal, halfVector), 0.0f);
					const float vDotH = std::max(dot(view, halfVector), 1.0e-5f);
					const float alpha = std::max(roughness * roughness, 0.001f);
					const float alpha2 = alpha * alpha;
					const float denominator = nDotH * nDotH * (alpha2 - 1.0f) + 1.0f;
					const float distribution = alpha2 /
						std::max(Pi * denominator * denominator, 1.0e-7f);
					const float pdf = std::max(distribution * nDotH / (4.0f * vDotH), 1.0e-6f);
					const float sampleSolidAngle = 1.0f / (static_cast<float>(sampleCount) * pdf);
					const float lod = roughness <= 0.001f ? 0.0f :
						0.5f * std::log2(std::max(sampleSolidAngle / texelSolidAngle, 1.0f));
					sum += sampleEquirectLod(sourceMips, light, lod) * nDotL;
					weight += nDotL;
				}
			}
			const Vec3 color = weight > 0.0f ? sum * (1.0f / weight) : Vec3{ 0, 0, 0 };
			const size_t offset = (static_cast<size_t>(y) * size + x) * 4;
			output[offset] = color.x; output[offset + 1] = color.y;
			output[offset + 2] = color.z; output[offset + 3] = 1.0f;
		}
	return output;
}

float geometrySchlick(float nDotV, float roughness)
{
	const float k = roughness * roughness * 0.5f;
	return nDotV / (nDotV * (1.0f - k) + k);
}

std::vector<float> createBrdfLut(int size, int sampleCount)
{
	std::vector<float> output(static_cast<size_t>(size) * size * 2);
	for(int y = 0; y < size; ++y)
		for(int x = 0; x < size; ++x)
		{
			const float nDotV = (static_cast<float>(x) + 0.5f) / size;
			const float roughness = (static_cast<float>(y) + 0.5f) / size;
			const Vec3 view{ std::sqrt(std::max(0.0f, 1.0f - nDotV * nDotV)), 0, nDotV };
			float a = 0.0f, b = 0.0f;
			for(int sample = 0; sample < sampleCount; ++sample)
			{
				float xiX, xiY;
				hammersley(static_cast<unsigned int>(sample), static_cast<unsigned int>(sampleCount), xiX, xiY);
				const Vec3 halfVector = importanceSampleGGX(xiX, xiY, roughness);
				const Vec3 light = normalize(halfVector * (2.0f * dot(view, halfVector)) - view);
				const float nDotL = std::max(light.z, 0.0f);
				const float nDotH = std::max(halfVector.z, 0.0f);
				const float vDotH = std::max(dot(view, halfVector), 0.0f);
				if(nDotL > 0.0f)
				{
					const float geometry = geometrySchlick(nDotV, roughness) * geometrySchlick(nDotL, roughness);
					const float visibility = geometry * vDotH / std::max(nDotH * nDotV, 1.0e-5f);
					const float fresnel = std::pow(1.0f - vDotH, 5.0f);
					a += (1.0f - fresnel) * visibility;
					b += fresnel * visibility;
				}
			}
			const size_t offset = (static_cast<size_t>(y) * size + x) * 2;
			output[offset] = a / sampleCount;
			output[offset + 1] = b / sampleCount;
		}
	return output;
}

bool upload(H3DRes resource, int imageElement, const std::vector<float> &pixels)
{
	void *mapped = h3dMapResStream(resource, H3DTexRes::ImageElem, imageElement,
		H3DTexRes::ImgPixelStream, false, true);
	if(mapped == nullptr) return false;
	std::memcpy(mapped, pixels.data(), pixels.size() * sizeof(float));
	h3dUnmapResStream(resource);
	return true;
}

H3DRes createTexture(const std::string &name, int size, int format, bool cubemap)
{
	int flags = H3DResFlags::NoQuery | H3DResFlags::NoTexCompression | H3DResFlags::NoTexMipmaps |
		H3DResFlags::TexDynamic;
	if(cubemap) flags |= H3DResFlags::TexCubemap;
	return h3dCreateTexture(name.c_str(), size, size, format, flags);
}

bool validSettings(const H3DHdriSettings &settings)
{
	return settings.environmentSize > 0 && settings.irradianceSize > 0 &&
		settings.specularSize > 0 && settings.brdfLutSize > 0 && settings.sampleCount > 0 &&
		settings.environmentSize <= 4096 && settings.irradianceSize <= 512 &&
		settings.specularSize <= 2048 && settings.brdfLutSize <= 1024 && settings.sampleCount <= 4096 &&
		settings.lightingSaturation >= 0.0f && settings.lightingSaturation <= 2.0f &&
		settings.lightingIntensity >= 0.0f && settings.lightingIntensity <= 16.0f;
}

}

void releaseIbl(H3DHdriIbl *ibl)
{
	if(ibl == nullptr) return;
	if(ibl->environment != 0) h3dRemoveResource(ibl->environment);
	if(ibl->irradiance != 0) h3dRemoveResource(ibl->irradiance);
	for(H3DRes &resource : ibl->specular) if(resource != 0) h3dRemoveResource(resource);
	if(ibl->brdfLut != 0) h3dRemoveResource(ibl->brdfLut);
	*ibl = H3DHdriIbl{};
}

bool createIbl(H3DRes equirectangularTexture, const char *resourcePrefix,
	const H3DHdriSettings *settingsPointer, H3DHdriIbl *outIbl)
{
	if(outIbl == nullptr || resourcePrefix == nullptr || resourcePrefix[0] == '\0') return false;
	const H3DHdriSettings settings = settingsPointer != nullptr ? *settingsPointer : H3DHdriSettings{};
	if(!validSettings(settings)) return false;
	if(h3dGetResType(equirectangularTexture) != H3DResTypes::Texture ||
		h3dGetResParamI(equirectangularTexture, H3DTexRes::TextureElem, 0, H3DTexRes::TexSliceCountI) != 1 ||
		h3dGetResParamI(equirectangularTexture, H3DTexRes::TextureElem, 0, H3DTexRes::TexFormatI) != H3DFormats::TEX_RGBA32F)
	{
		Horde3D::Modules::log().writeError("HDRI: source must be a loaded 2D RGBA32F texture (Radiance .hdr is supported)");
		return false;
	}

	FloatImage source;
	source.width = h3dGetResParamI(equirectangularTexture, H3DTexRes::ImageElem, 0, H3DTexRes::ImgWidthI);
	source.height = h3dGetResParamI(equirectangularTexture, H3DTexRes::ImageElem, 0, H3DTexRes::ImgHeightI);
	if(source.width < 2 || source.height < 2) return false;
	const float *sourcePixels = static_cast<const float *>(h3dMapResStream(equirectangularTexture,
		H3DTexRes::ImageElem, 0, H3DTexRes::ImgPixelStream, true, false));
	if(sourcePixels == nullptr) return false;
	source.rgba.assign(sourcePixels, sourcePixels + static_cast<size_t>(source.width) * source.height * 4);
	h3dUnmapResStream(equirectangularTexture);
	const FloatImage lightingSource = withLightingAdjustments(source, settings.lightingSaturation,
		settings.lightingIntensity);
	const std::vector<FloatImage> sourceMips = buildMipChain(lightingSource);

	H3DHdriIbl generated;
	const std::string prefix(resourcePrefix);
	generated.environment = createTexture(prefix + ".environment", settings.environmentSize,
		H3DFormats::TEX_RGBA32F, true);
	generated.irradiance = createTexture(prefix + ".irradiance", settings.irradianceSize,
		H3DFormats::TEX_RGBA32F, true);
	generated.brdfLut = createTexture(prefix + ".brdfLut", settings.brdfLutSize,
		H3DFormats::TEX_RG32F, false);
	for(int level = 0; level < H3D_HDRI_SPECULAR_LEVELS; ++level)
	{
		const int levelSize = std::max(1, settings.specularSize >> level);
		generated.specular[level] = createTexture(prefix + ".specular" + std::to_string(level),
			levelSize, H3DFormats::TEX_RGBA32F, true);
	}
	if(generated.environment == 0 || generated.irradiance == 0 || generated.brdfLut == 0 ||
		std::any_of(std::begin(generated.specular), std::end(generated.specular), [](H3DRes res) { return res == 0; }))
	{
		releaseIbl(&generated);
		return false;
	}

	for(int face = 0; face < 6; ++face)
	{
		if(!upload(generated.environment, face, environmentFace(source, face, settings.environmentSize)) ||
			!upload(generated.irradiance, face, irradianceFace(lightingSource, face, settings.irradianceSize,
				settings.sampleCount)))
		{
			releaseIbl(&generated);
			return false;
		}
		for(int level = 0; level < H3D_HDRI_SPECULAR_LEVELS; ++level)
		{
			const int levelSize = std::max(1, settings.specularSize >> level);
			const float roughness = static_cast<float>(level) / (H3D_HDRI_SPECULAR_LEVELS - 1);
			if(!upload(generated.specular[level], face,
				specularFace(sourceMips, face, levelSize, settings.sampleCount, roughness)))
			{
				releaseIbl(&generated);
				return false;
			}
		}
	}
	if(!upload(generated.brdfLut, 0, createBrdfLut(settings.brdfLutSize, settings.sampleCount)))
	{
		releaseIbl(&generated);
		return false;
	}

	*outIbl = generated;
	Horde3D::Modules::log().writeInfo("HDRI: generated IBL resources from '%s'", resourcePrefix);
	return true;
}

bool bindIbl(H3DRes material, const H3DHdriIbl *ibl)
{
	if(ibl == nullptr || h3dGetResType(material) != H3DResTypes::Material) return false;
	int boundMask = 0;
	const int samplerCount = h3dGetResElemCount(material, H3DMatRes::SamplerElem);
	for(int sampler = 0; sampler < samplerCount; ++sampler)
	{
		const char *name = h3dGetResParamStr(material, H3DMatRes::SamplerElem, sampler, H3DMatRes::SampNameStr);
		H3DRes resource = 0;
		int bit = -1;
		if(std::strcmp(name, "iblIrradiance") == 0) { resource = ibl->irradiance; bit = 0; }
		else if(std::strcmp(name, "iblBrdfLut") == 0) { resource = ibl->brdfLut; bit = 6; }
		else
			for(int level = 0; level < H3D_HDRI_SPECULAR_LEVELS; ++level)
				if(std::strcmp(name, (std::string("iblSpecular") + std::to_string(level)).c_str()) == 0)
				{ resource = ibl->specular[level]; bit = level + 1; break; }
		if(bit >= 0 && resource != 0)
		{
			h3dSetResParamI(material, H3DMatRes::SamplerElem, sampler, H3DMatRes::SampTexResI, resource);
			boundMask |= 1 << bit;
		}
	}
	return boundMask == 0x7f;
}

}
