[[FX]]

samplerCube iblIrradiance = sampler_state { Address = Clamp; Filter = Bilinear; };
samplerCube iblSpecular0 = sampler_state { Address = Clamp; Filter = Bilinear; };
samplerCube iblSpecular1 = sampler_state { Address = Clamp; Filter = Bilinear; };
samplerCube iblSpecular2 = sampler_state { Address = Clamp; Filter = Bilinear; };
samplerCube iblSpecular3 = sampler_state { Address = Clamp; Filter = Bilinear; };
samplerCube iblSpecular4 = sampler_state { Address = Clamp; Filter = Bilinear; };
sampler2D iblBrdfLut = sampler_state { Address = Clamp; Filter = Bilinear; };

float4 baseColor < string desc_abc = "linear base color"; > = { 1.0, 1.0, 1.0, 1.0 };
float4 pbrParams < string desc_abc = "metallic, roughness, ambient occlusion"; string desc_d = "IBL intensity"; >
	= { 0.0, 0.5, 1.0, 1.0 };

context AMBIENT
{
	VertexShader = compile GLSL VS_PBR;
	PixelShader = compile GLSL FS_PBR;
}

context SHADOWMAP
{
	VertexShader = compile GLSL VS_PBR;
	PixelShader = compile GLSL FS_SHADOW;
}

OpenGL4
{
	context AMBIENT
	{
		VertexShader = compile GLSL VS_PBR_GL4;
		PixelShader = compile GLSL FS_PBR_GL4;
	}
	context SHADOWMAP
	{
		VertexShader = compile GLSL VS_PBR_GL4;
		PixelShader = compile GLSL FS_SHADOW_GL4;
	}
}

OpenGLES3
{
	context AMBIENT
	{
		VertexShader = compile GLSL VS_PBR_GLES3;
		PixelShader = compile GLSL FS_PBR_GLES3;
	}
	context SHADOWMAP
	{
		VertexShader = compile GLSL VS_PBR_GLES3;
		PixelShader = compile GLSL FS_SHADOW_GLES3;
	}
}

[[VS_PBR]]
#include "shaders/utilityLib/vertCommon.glsl"
uniform mat4 viewProjMat;
attribute vec3 vertPos;
attribute vec3 normal;
varying vec3 worldPosition;
varying vec3 worldNormal;
void main()
{
	vec4 position = calcWorldPos(vec4(vertPos, 1.0));
	worldPosition = position.xyz;
	worldNormal = normalize(calcWorldVec(normal));
	gl_Position = viewProjMat * position;
}

[[FS_PBR]]
uniform vec3 viewerPos;
uniform vec4 baseColor;
uniform vec4 pbrParams;
uniform samplerCube iblIrradiance;
uniform samplerCube iblSpecular0;
uniform samplerCube iblSpecular1;
uniform samplerCube iblSpecular2;
uniform samplerCube iblSpecular3;
uniform samplerCube iblSpecular4;
uniform sampler2D iblBrdfLut;
varying vec3 worldPosition;
varying vec3 worldNormal;

vec3 fresnelRoughness(float cosine, vec3 f0, float roughness)
{
	return f0 + (max(vec3(1.0 - roughness), f0) - f0) * pow(1.0 - cosine, 5.0);
}

vec3 samplePrefilter(vec3 direction, float roughness)
{
	float level = clamp(roughness, 0.0, 1.0) * 4.0;
	if(level < 1.0) return mix(textureCube(iblSpecular0, direction).rgb, textureCube(iblSpecular1, direction).rgb, level);
	if(level < 2.0) return mix(textureCube(iblSpecular1, direction).rgb, textureCube(iblSpecular2, direction).rgb, level - 1.0);
	if(level < 3.0) return mix(textureCube(iblSpecular2, direction).rgb, textureCube(iblSpecular3, direction).rgb, level - 2.0);
	return mix(textureCube(iblSpecular3, direction).rgb, textureCube(iblSpecular4, direction).rgb, level - 3.0);
}

void main()
{
	float metallic = clamp(pbrParams.x, 0.0, 1.0);
	float roughness = clamp(pbrParams.y, 0.02, 1.0);
	vec3 normal = normalize(worldNormal);
	vec3 view = normalize(viewerPos - worldPosition);
	float nDotV = max(dot(normal, view), 0.0);
	vec3 f0 = mix(vec3(0.04), baseColor.rgb, metallic);
	vec3 fresnel = fresnelRoughness(nDotV, f0, roughness);
	vec3 diffuse = textureCube(iblIrradiance, normal).rgb * baseColor.rgb;
	vec3 reflected = samplePrefilter(reflect(-view, normal), roughness);
	vec2 brdf = texture2D(iblBrdfLut, vec2(nDotV, roughness)).rg;
	vec3 specular = reflected * (fresnel * brdf.x + brdf.y);
	vec3 kd = (vec3(1.0) - fresnel) * (1.0 - metallic);
	gl_FragColor = vec4((kd * diffuse + specular) * pbrParams.z * pbrParams.w, baseColor.a);
}

[[FS_SHADOW]]
void main() {}

[[VS_PBR_GL4]]
#include "shaders/utilityLib/vertCommon.glsl"
uniform mat4 viewProjMat;
layout(location = 0) in vec3 vertPos;
layout(location = 1) in vec3 normal;
out vec3 worldPosition;
out vec3 worldNormal;
void main()
{
	vec4 position = calcWorldPos(vec4(vertPos, 1.0));
	worldPosition = position.xyz;
	worldNormal = normalize(calcWorldVec(normal));
	gl_Position = viewProjMat * position;
}

[[FS_PBR_GL4]]
uniform vec3 viewerPos;
uniform vec4 baseColor;
uniform vec4 pbrParams;
uniform samplerCube iblIrradiance;
uniform samplerCube iblSpecular0;
uniform samplerCube iblSpecular1;
uniform samplerCube iblSpecular2;
uniform samplerCube iblSpecular3;
uniform samplerCube iblSpecular4;
uniform sampler2D iblBrdfLut;
in vec3 worldPosition;
in vec3 worldNormal;
out vec4 fragColor;

vec3 fresnelRoughness(float cosine, vec3 f0, float roughness)
{
	return f0 + (max(vec3(1.0 - roughness), f0) - f0) * pow(1.0 - cosine, 5.0);
}

vec3 samplePrefilter(vec3 direction, float roughness)
{
	float level = clamp(roughness, 0.0, 1.0) * 4.0;
	if(level < 1.0) return mix(texture(iblSpecular0, direction).rgb, texture(iblSpecular1, direction).rgb, level);
	if(level < 2.0) return mix(texture(iblSpecular1, direction).rgb, texture(iblSpecular2, direction).rgb, level - 1.0);
	if(level < 3.0) return mix(texture(iblSpecular2, direction).rgb, texture(iblSpecular3, direction).rgb, level - 2.0);
	return mix(texture(iblSpecular3, direction).rgb, texture(iblSpecular4, direction).rgb, level - 3.0);
}

void main()
{
	float metallic = clamp(pbrParams.x, 0.0, 1.0);
	float roughness = clamp(pbrParams.y, 0.02, 1.0);
	vec3 normal = normalize(worldNormal);
	vec3 view = normalize(viewerPos - worldPosition);
	float nDotV = max(dot(normal, view), 0.0);
	vec3 f0 = mix(vec3(0.04), baseColor.rgb, metallic);
	vec3 fresnel = fresnelRoughness(nDotV, f0, roughness);
	vec3 diffuse = texture(iblIrradiance, normal).rgb * baseColor.rgb;
	vec3 reflected = samplePrefilter(reflect(-view, normal), roughness);
	vec2 brdf = texture(iblBrdfLut, vec2(nDotV, roughness)).rg;
	vec3 specular = reflected * (fresnel * brdf.x + brdf.y);
	vec3 kd = (vec3(1.0) - fresnel) * (1.0 - metallic);
	fragColor = vec4((kd * diffuse + specular) * pbrParams.z * pbrParams.w, baseColor.a);
}

[[FS_SHADOW_GL4]]
out vec4 fragColor;
void main() { fragColor = vec4(1.0); }

[[VS_PBR_GLES3]]
#include "shaders/utilityLib/vertCommon.glsl"
uniform mat4 viewProjMat;
layout(location = 0) in vec3 vertPos;
layout(location = 1) in vec3 normal;
out vec3 worldPosition;
out vec3 worldNormal;
void main()
{
	vec4 position = calcWorldPos(vec4(vertPos, 1.0));
	worldPosition = position.xyz;
	worldNormal = normalize(calcWorldVec(normal));
	gl_Position = viewProjMat * position;
}

[[FS_PBR_GLES3]]
precision highp float;
uniform vec3 viewerPos;
uniform vec4 baseColor;
uniform vec4 pbrParams;
uniform samplerCube iblIrradiance;
uniform samplerCube iblSpecular0;
uniform samplerCube iblSpecular1;
uniform samplerCube iblSpecular2;
uniform samplerCube iblSpecular3;
uniform samplerCube iblSpecular4;
uniform sampler2D iblBrdfLut;
in vec3 worldPosition;
in vec3 worldNormal;
out vec4 fragColor;

vec3 fresnelRoughness(float cosine, vec3 f0, float roughness)
{
	return f0 + (max(vec3(1.0 - roughness), f0) - f0) * pow(1.0 - cosine, 5.0);
}

vec3 samplePrefilter(vec3 direction, float roughness)
{
	float level = clamp(roughness, 0.0, 1.0) * 4.0;
	if(level < 1.0) return mix(texture(iblSpecular0, direction).rgb, texture(iblSpecular1, direction).rgb, level);
	if(level < 2.0) return mix(texture(iblSpecular1, direction).rgb, texture(iblSpecular2, direction).rgb, level - 1.0);
	if(level < 3.0) return mix(texture(iblSpecular2, direction).rgb, texture(iblSpecular3, direction).rgb, level - 2.0);
	return mix(texture(iblSpecular3, direction).rgb, texture(iblSpecular4, direction).rgb, level - 3.0);
}

void main()
{
	float metallic = clamp(pbrParams.x, 0.0, 1.0);
	float roughness = clamp(pbrParams.y, 0.02, 1.0);
	vec3 normal = normalize(worldNormal);
	vec3 view = normalize(viewerPos - worldPosition);
	float nDotV = max(dot(normal, view), 0.0);
	vec3 f0 = mix(vec3(0.04), baseColor.rgb, metallic);
	vec3 fresnel = fresnelRoughness(nDotV, f0, roughness);
	vec3 diffuse = texture(iblIrradiance, normal).rgb * baseColor.rgb;
	vec3 reflected = samplePrefilter(reflect(-view, normal), roughness);
	vec2 brdf = texture(iblBrdfLut, vec2(nDotV, roughness)).rg;
	vec3 specular = reflected * (fresnel * brdf.x + brdf.y);
	vec3 kd = (vec3(1.0) - fresnel) * (1.0 - metallic);
	fragColor = vec4((kd * diffuse + specular) * pbrParams.z * pbrParams.w, baseColor.a);
}

[[FS_SHADOW_GLES3]]
precision highp float;
out vec4 fragColor;
void main() { fragColor = vec4(1.0); }
