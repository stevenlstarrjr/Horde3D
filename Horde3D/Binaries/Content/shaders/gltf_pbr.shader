[[FX]]

sampler2D baseColorMap = sampler_state { Texture = "textures/common/white.tga"; };
sampler2D metallicRoughnessMap = sampler_state { Texture = "textures/common/white.tga"; };
sampler2D normalMap = sampler_state { Texture = "textures/common/defnorm.tga"; };
sampler2D occlusionMap = sampler_state { Texture = "textures/common/white.tga"; };
sampler2D emissiveMap = sampler_state { Texture = "textures/common/white.tga"; };
sampler2D iridescenceMap = sampler_state { Texture = "textures/common/white.tga"; };
sampler2D iridescenceThicknessMap = sampler_state { Texture = "textures/common/white.tga"; };
sampler2D transmissionMap = sampler_state { Texture = "textures/common/white.tga"; };
sampler2D volumeThicknessMap = sampler_state { Texture = "textures/common/white.tga"; };
sampler2D sceneColor = sampler_state { Address = Clamp; Filter = Trilinear; };
samplerCube iblIrradiance = sampler_state { Address = Clamp; Filter = Bilinear; };
samplerCube iblSpecular0 = sampler_state { Address = Clamp; Filter = Bilinear; };
samplerCube iblSpecular1 = sampler_state { Address = Clamp; Filter = Bilinear; };
samplerCube iblSpecular2 = sampler_state { Address = Clamp; Filter = Bilinear; };
samplerCube iblSpecular3 = sampler_state { Address = Clamp; Filter = Bilinear; };
sampler2D iblBrdfLut = sampler_state { Address = Clamp; Filter = Bilinear; };

float4 baseColorFactor = { 1.0, 1.0, 1.0, 1.0 };
float4 pbrParams = { 1.0, 1.0, 1.0, 1.0 };
float4 emissiveFactor = { 0.0, 0.0, 0.0, 0.0 };
float normalScale = 1.0;
float pbrDebugView = 0.0;
float baseIor = 1.5;
float4 iridescenceParams = { 0.0, 1.3, 100.0, 400.0 };
float4 transmissionParams = { 0.0, 0.0, 1.0e20, 0.0 };
float4 attenuationColor = { 1.0, 1.0, 1.0, 1.0 };
float4 clearcoatParams = { 0.0, 0.0, 1.0, 0.0 };
float4 anisotropyParams = { 0.0, 0.0, 0.0, 0.0 };
float4 baseColorTexTransform = { 0.0, 0.0, 1.0, 1.0 };
float4 metalRoughTexTransform = { 0.0, 0.0, 1.0, 1.0 };
float4 normalTexTransform = { 0.0, 0.0, 1.0, 1.0 };
float4 occlusionTexTransform = { 0.0, 0.0, 1.0, 1.0 };
float4 emissiveTexTransform = { 0.0, 0.0, 1.0, 1.0 };
float4 iridescenceTexTransform = { 0.0, 0.0, 1.0, 1.0 };
float4 iridescenceThicknessTexTransform = { 0.0, 0.0, 1.0, 1.0 };
float4 transmissionTexTransform = { 0.0, 0.0, 1.0, 1.0 };
float4 volumeThicknessTexTransform = { 0.0, 0.0, 1.0, 1.0 };
float baseColorTexRotation = 0.0;
float metalRoughTexRotation = 0.0;
float normalTexRotation = 0.0;
float occlusionTexRotation = 0.0;
float emissiveTexRotation = 0.0;
float iridescenceTexRotation = 0.0;
float iridescenceThicknessTexRotation = 0.0;
float transmissionTexRotation = 0.0;
float volumeThicknessTexRotation = 0.0;

context AMBIENT
{
	VertexShader = compile GLSL VS_GLTF;
	PixelShader = compile GLSL FS_GLTF;
}

context TRANSLUCENT
{
	VertexShader = compile GLSL VS_GLTF;
	PixelShader = compile GLSL FS_GLTF;
	ZWriteEnable = false;
	BlendMode = Replace;
}

context LIGHTING
{
	VertexShader = compile GLSL VS_GLTF;
	PixelShader = compile GLSL FS_GLTF_LIGHTING;
	ZWriteEnable = false;
	BlendMode = Add;
}

context SHADOWMAP
{
	VertexShader = compile GLSL VS_GLTF;
	PixelShader = compile GLSL FS_SHADOW;
}

OpenGL4
{
	context AMBIENT
	{
		VertexShader = compile GLSL VS_GLTF_GL4;
		PixelShader = compile GLSL FS_GLTF_GL4;
	}
	context TRANSLUCENT
	{
		VertexShader = compile GLSL VS_GLTF_GL4;
		PixelShader = compile GLSL FS_GLTF_GL4;
		ZWriteEnable = false;
		BlendMode = Replace;
	}
	context LIGHTING
	{
		VertexShader = compile GLSL VS_GLTF_GL4;
		PixelShader = compile GLSL FS_GLTF_LIGHTING_GL4;
		ZWriteEnable = false;
		BlendMode = Add;
	}
	context SHADOWMAP
	{
		VertexShader = compile GLSL VS_GLTF_GL4;
		PixelShader = compile GLSL FS_SHADOW_GL4;
	}
}

OpenGLES3
{
	context AMBIENT
	{
		VertexShader = compile GLSL VS_GLTF_GLES3;
		PixelShader = compile GLSL FS_GLTF_GLES3;
	}
	context TRANSLUCENT
	{
		VertexShader = compile GLSL VS_GLTF_GLES3;
		PixelShader = compile GLSL FS_GLTF_GLES3;
		ZWriteEnable = false;
		BlendMode = Replace;
	}
	context LIGHTING
	{
		VertexShader = compile GLSL VS_GLTF_GLES3;
		PixelShader = compile GLSL FS_GLTF_LIGHTING_GLES3;
		ZWriteEnable = false;
		BlendMode = Add;
	}
	context SHADOWMAP
	{
		VertexShader = compile GLSL VS_GLTF_GLES3;
		PixelShader = compile GLSL FS_SHADOW_GLES3;
	}
}

[[VS_GLTF]]
#include "shaders/utilityLib/vertCommon.glsl"
uniform mat4 viewProjMat;
attribute vec3 vertPos;
attribute vec3 normal;
attribute vec4 tangent;
attribute vec2 texCoords0;
varying vec3 worldPosition;
varying mat3 tangentToWorld;
varying vec2 texCoord;
varying vec4 viewPosition;
void main()
{
	vec4 position = calcWorldPos(vec4(vertPos, 1.0));
	vec3 n = normalize(calcWorldVec(normal));
	vec3 t = normalize(calcWorldVec(tangent.xyz));
	vec3 b = normalize(cross(n, t)) * tangent.w;
	worldPosition = position.xyz;
	tangentToWorld = mat3(t, b, n);
	texCoord = texCoords0;
	viewPosition = calcViewPos(position);
	gl_Position = viewProjMat * position;
}

[[FS_GLTF]]
uniform vec3 viewerPos;
uniform vec4 baseColorFactor;
uniform vec4 pbrParams;
uniform vec4 emissiveFactor;
uniform float normalScale;
uniform sampler2D baseColorMap;
uniform sampler2D metallicRoughnessMap;
uniform sampler2D normalMap;
uniform sampler2D occlusionMap;
uniform sampler2D emissiveMap;
uniform samplerCube iblIrradiance;
uniform samplerCube iblSpecular0;
uniform samplerCube iblSpecular1;
uniform samplerCube iblSpecular2;
uniform samplerCube iblSpecular3;
uniform sampler2D iblBrdfLut;
varying vec3 worldPosition;
varying mat3 tangentToWorld;
varying vec2 texCoord;
#define GLTF_TEX texture2D
#define GLTF_TEX_LOD(s, uv, lod) texture2D(s, uv, lod)
#define GLTF_CUBE textureCube
#include "shaders/utilityLib/fragGltfPbr.glsl"
void main() { gl_FragColor = shadeGltfPbr(); }

[[FS_GLTF_LIGHTING]]
#include "shaders/utilityLib/fragLighting.glsl"
uniform vec4 baseColorFactor;
uniform vec4 pbrParams;
uniform vec4 emissiveFactor;
uniform float normalScale;
uniform sampler2D baseColorMap;
uniform sampler2D metallicRoughnessMap;
uniform sampler2D normalMap;
uniform sampler2D occlusionMap;
uniform sampler2D emissiveMap;
uniform samplerCube iblIrradiance;
uniform samplerCube iblSpecular0;
uniform samplerCube iblSpecular1;
uniform samplerCube iblSpecular2;
uniform samplerCube iblSpecular3;
uniform sampler2D iblBrdfLut;
varying vec3 worldPosition;
varying mat3 tangentToWorld;
varying vec2 texCoord;
varying vec4 viewPosition;
#define GLTF_TEX texture2D
#define GLTF_TEX_LOD(s, uv, lod) texture2D(s, uv, lod)
#define GLTF_CUBE textureCube
#include "shaders/utilityLib/fragGltfPbr.glsl"
#include "shaders/utilityLib/fragGltfPbrLighting.glsl"
void main() { gl_FragColor = vec4(shadeGltfPbrLight(-viewPosition.z), 0.0); }

[[FS_SHADOW]]
void main() {}

[[VS_GLTF_GL4]]
#include "shaders/utilityLib/vertCommon.glsl"
uniform mat4 viewProjMat;
layout(location = 0) in vec3 vertPos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec4 tangent;
layout(location = 5) in vec2 texCoords0;
out vec3 worldPosition;
out mat3 tangentToWorld;
out vec2 texCoord;
out vec4 viewPosition;
void main()
{
	vec4 position = calcWorldPos(vec4(vertPos, 1.0));
	vec3 n = normalize(calcWorldVec(normal));
	vec3 t = normalize(calcWorldVec(tangent.xyz));
	vec3 b = normalize(cross(n, t)) * tangent.w;
	worldPosition = position.xyz;
	tangentToWorld = mat3(t, b, n);
	texCoord = texCoords0;
	viewPosition = calcViewPos(position);
	gl_Position = viewProjMat * position;
}

[[FS_GLTF_GL4]]
uniform vec3 viewerPos;
uniform vec4 baseColorFactor;
uniform vec4 pbrParams;
uniform vec4 emissiveFactor;
uniform float normalScale;
uniform sampler2D baseColorMap;
uniform sampler2D metallicRoughnessMap;
uniform sampler2D normalMap;
uniform sampler2D occlusionMap;
uniform sampler2D emissiveMap;
uniform samplerCube iblIrradiance;
uniform samplerCube iblSpecular0;
uniform samplerCube iblSpecular1;
uniform samplerCube iblSpecular2;
uniform samplerCube iblSpecular3;
uniform sampler2D iblBrdfLut;
in vec3 worldPosition;
in mat3 tangentToWorld;
in vec2 texCoord;
out vec4 fragColor;
#define GLTF_TEX texture
#define GLTF_TEX_LOD textureLod
#define GLTF_CUBE texture
#include "shaders/utilityLib/fragGltfPbr.glsl"
void main() { fragColor = shadeGltfPbr(); }

[[FS_GLTF_LIGHTING_GL4]]
#include "shaders/utilityLib/fragLightingGL4.glsl"
uniform vec4 baseColorFactor;
uniform vec4 pbrParams;
uniform vec4 emissiveFactor;
uniform float normalScale;
uniform sampler2D baseColorMap;
uniform sampler2D metallicRoughnessMap;
uniform sampler2D normalMap;
uniform sampler2D occlusionMap;
uniform sampler2D emissiveMap;
uniform samplerCube iblIrradiance;
uniform samplerCube iblSpecular0;
uniform samplerCube iblSpecular1;
uniform samplerCube iblSpecular2;
uniform samplerCube iblSpecular3;
uniform sampler2D iblBrdfLut;
in vec3 worldPosition;
in mat3 tangentToWorld;
in vec2 texCoord;
in vec4 viewPosition;
out vec4 fragColor;
#define GLTF_TEX texture
#define GLTF_TEX_LOD textureLod
#define GLTF_CUBE texture
#include "shaders/utilityLib/fragGltfPbr.glsl"
#include "shaders/utilityLib/fragGltfPbrLighting.glsl"
void main() { fragColor = vec4(shadeGltfPbrLight(-viewPosition.z), 0.0); }

[[FS_SHADOW_GL4]]
out vec4 fragColor;
void main() { fragColor = vec4(1.0); }

[[VS_GLTF_GLES3]]
#include "shaders/utilityLib/vertCommon.glsl"
uniform mat4 viewProjMat;
layout(location = 0) in vec3 vertPos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec4 tangent;
layout(location = 5) in vec2 texCoords0;
out vec3 worldPosition;
out mat3 tangentToWorld;
out vec2 texCoord;
out vec4 viewPosition;
void main()
{
	vec4 position = calcWorldPos(vec4(vertPos, 1.0));
	vec3 n = normalize(calcWorldVec(normal));
	vec3 t = normalize(calcWorldVec(tangent.xyz));
	vec3 b = normalize(cross(n, t)) * tangent.w;
	worldPosition = position.xyz;
	tangentToWorld = mat3(t, b, n);
	texCoord = texCoords0;
	viewPosition = calcViewPos(position);
	gl_Position = viewProjMat * position;
}

[[FS_GLTF_GLES3]]
precision highp float;
uniform vec3 viewerPos;
uniform vec4 baseColorFactor;
uniform vec4 pbrParams;
uniform vec4 emissiveFactor;
uniform float normalScale;
uniform sampler2D baseColorMap;
uniform sampler2D metallicRoughnessMap;
uniform sampler2D normalMap;
uniform sampler2D occlusionMap;
uniform sampler2D emissiveMap;
uniform samplerCube iblIrradiance;
uniform samplerCube iblSpecular0;
uniform samplerCube iblSpecular1;
uniform samplerCube iblSpecular2;
uniform samplerCube iblSpecular3;
uniform sampler2D iblBrdfLut;
in vec3 worldPosition;
in mat3 tangentToWorld;
in vec2 texCoord;
out vec4 fragColor;
#define GLTF_TEX texture
#define GLTF_TEX_LOD textureLod
#define GLTF_CUBE texture
#include "shaders/utilityLib/fragGltfPbr.glsl"
void main() { fragColor = shadeGltfPbr(); }

[[FS_GLTF_LIGHTING_GLES3]]
precision highp float;
#include "shaders/utilityLib/fragLightingGLES3.glsl"
uniform vec4 baseColorFactor;
uniform vec4 pbrParams;
uniform vec4 emissiveFactor;
uniform float normalScale;
uniform sampler2D baseColorMap;
uniform sampler2D metallicRoughnessMap;
uniform sampler2D normalMap;
uniform sampler2D occlusionMap;
uniform sampler2D emissiveMap;
uniform samplerCube iblIrradiance;
uniform samplerCube iblSpecular0;
uniform samplerCube iblSpecular1;
uniform samplerCube iblSpecular2;
uniform samplerCube iblSpecular3;
uniform sampler2D iblBrdfLut;
in vec3 worldPosition;
in mat3 tangentToWorld;
in vec2 texCoord;
in vec4 viewPosition;
out vec4 fragColor;
#define GLTF_TEX texture
#define GLTF_TEX_LOD textureLod
#define GLTF_CUBE texture
#include "shaders/utilityLib/fragGltfPbr.glsl"
#include "shaders/utilityLib/fragGltfPbrLighting.glsl"
void main() { fragColor = vec4(shadeGltfPbrLight(-viewPosition.z), 0.0); }

[[FS_SHADOW_GLES3]]
precision highp float;
out vec4 fragColor;
void main() { fragColor = vec4(1.0); }
