[[FX]]

// An HDR environment for LDR/deferred pipelines. It tonemaps the visible
// skybox only; IBL generation still receives the original HDR radiance.
samplerCube albedoMap = sampler_state
{
	Address = Clamp;
};

float hdrExposure = 0.18;

context ATTRIBPASS
{
	VertexShader = compile GLSL VS_GENERAL;
	PixelShader = compile GLSL FS_ATTRIBPASS;
}

context AMBIENT
{
	VertexShader = compile GLSL VS_GENERAL;
	PixelShader = compile GLSL FS_AMBIENT;
}

OpenGL4
{
	context ATTRIBPASS
	{
		VertexShader = compile GLSL VS_GENERAL_GL4;
		PixelShader = compile GLSL FS_ATTRIBPASS_GL4;
	}

	context AMBIENT
	{
		VertexShader = compile GLSL VS_GENERAL_GL4;
		PixelShader = compile GLSL FS_AMBIENT_GL4;
	}
}

OpenGLES3
{
	context ATTRIBPASS
	{
		VertexShader = compile GLSL VS_GENERAL_GL4;
		PixelShader = compile GLSL FS_ATTRIBPASS_GL4;
	}

	context AMBIENT
	{
		VertexShader = compile GLSL VS_GENERAL_GL4;
		PixelShader = compile GLSL FS_AMBIENT_GL4;
	}
}

[[VS_GENERAL]]

#include "shaders/utilityLib/vertCommon.glsl"

uniform mat4 viewProjMat;
uniform vec3 viewerPos;
attribute vec3 vertPos;
varying vec3 viewVec;

void main(void)
{
	vec4 pos = calcWorldPos(vec4(vertPos, 1.0));
	viewVec = pos.xyz - viewerPos;
	gl_Position = viewProjMat * pos;
}

[[VS_GENERAL_GL4]]

#include "shaders/utilityLib/vertCommon.glsl"

uniform mat4 viewProjMat;
uniform vec3 viewerPos;
layout(location = 0) in vec3 vertPos;
out vec3 viewVec;

void main(void)
{
	vec4 pos = calcWorldPos(vec4(vertPos, 1.0));
	viewVec = pos.xyz - viewerPos;
	gl_Position = viewProjMat * pos;
}

[[FS_ATTRIBPASS]]

#include "shaders/utilityLib/fragDeferredWrite.glsl"

uniform samplerCube albedoMap;
uniform float hdrExposure;
varying vec3 viewVec;

vec3 acesFilm(vec3 color)
{
	const float a = 2.51;
	const float b = 0.03;
	const float c = 2.43;
	const float d = 0.59;
	const float e = 0.14;
	return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

vec3 displayColor(vec3 hdrColor)
{
	return pow(acesFilm(max(hdrColor, 0.0) * hdrExposure), vec3(1.0 / 2.2));
}

void main(void)
{
	setMatID(2.0);
	setAlbedo(displayColor(textureCube(albedoMap, viewVec).rgb));
}

[[FS_ATTRIBPASS_GL4]]

#include "shaders/utilityLib/fragDeferredWriteGL4.glsl"

uniform samplerCube albedoMap;
uniform float hdrExposure;
in vec3 viewVec;

vec3 acesFilm(vec3 color)
{
	const float a = 2.51;
	const float b = 0.03;
	const float c = 2.43;
	const float d = 0.59;
	const float e = 0.14;
	return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

vec3 displayColor(vec3 hdrColor)
{
	return pow(acesFilm(max(hdrColor, 0.0) * hdrExposure), vec3(1.0 / 2.2));
}

void main(void)
{
	setMatID(2.0);
	setAlbedo(displayColor(texture(albedoMap, viewVec).rgb));
}

[[FS_AMBIENT]]

uniform samplerCube albedoMap;
uniform float hdrExposure;
varying vec3 viewVec;

vec3 acesFilm(vec3 color)
{
	const float a = 2.51;
	const float b = 0.03;
	const float c = 2.43;
	const float d = 0.59;
	const float e = 0.14;
	return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

void main(void)
{
	vec3 displayColor = pow(acesFilm(max(textureCube(albedoMap, viewVec).rgb, 0.0) * hdrExposure), vec3(1.0 / 2.2));
	gl_FragColor.rgb = displayColor;
}

[[FS_AMBIENT_GL4]]

uniform samplerCube albedoMap;
uniform float hdrExposure;
in vec3 viewVec;
out vec4 fragColor;

vec3 acesFilm(vec3 color)
{
	const float a = 2.51;
	const float b = 0.03;
	const float c = 2.43;
	const float d = 0.59;
	const float e = 0.14;
	return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

void main(void)
{
	vec3 displayColor = pow(acesFilm(max(texture(albedoMap, viewVec).rgb, 0.0) * hdrExposure), vec3(1.0 / 2.2));
	fragColor.rgb = displayColor;
}
