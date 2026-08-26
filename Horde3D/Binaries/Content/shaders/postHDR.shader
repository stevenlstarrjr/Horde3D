[[FX]]

// Samplers
sampler2D buf0 = sampler_state
{
	Address = Clamp;
};

sampler2D buf1 = sampler_state
{
	Address = Clamp;
};

// Uniforms
float hdrExposure = 2.0;       // Exposure (higher values make scene brighter)
float hdrBrightThres = 0.6;    // Brightpass threshold (intensity where blooming begins)
float hdrBrightOffset = 0.06;  // Brightpass offset (smaller values produce stronger blooming)
float hdrBloomStrength = 0.15; // Contribution of blurred highlights in the final image

float4 blurParams = {0, 0, 0, 0};

// Contexts
context COPY
{
	VertexShader = compile GLSL VS_FSQUAD;
	PixelShader = compile GLSL FS_COPY;
	ZWriteEnable = false;
}

context BRIGHTPASS
{
	VertexShader = compile GLSL VS_FSQUAD;
	PixelShader = compile GLSL FS_BRIGHTPASS;
	
	ZWriteEnable = false;
}

context BLUR
{
	VertexShader = compile GLSL VS_FSQUAD;
	PixelShader = compile GLSL FS_BLUR;
	
	ZWriteEnable = false;
}

context FINALPASS
{
	VertexShader = compile GLSL VS_FSQUAD;
	PixelShader = compile GLSL FS_FINALPASS;
	
	ZWriteEnable = false;
}

OpenGL4
{
	context COPY
	{
		VertexShader = compile GLSL VS_FSQUAD_GL4;
		PixelShader = compile GLSL FS_COPY_GL4;
		ZWriteEnable = false;
	}

	context BRIGHTPASS
	{
		VertexShader = compile GLSL VS_FSQUAD_GL4;
		PixelShader = compile GLSL FS_BRIGHTPASS_GL4;
		
		ZWriteEnable = false;
	}

	context BLUR
	{
		VertexShader = compile GLSL VS_FSQUAD_GL4;
		PixelShader = compile GLSL FS_BLUR_GL4;
		
		ZWriteEnable = false;
	}

	context FINALPASS
	{
		VertexShader = compile GLSL VS_FSQUAD_GL4;
		PixelShader = compile GLSL FS_FINALPASS_GL4;
		
		ZWriteEnable = false;
	}
}

OpenGLES3
{
	context COPY
	{
		VertexShader = compile GLSL VS_FSQUAD_GL4;
		PixelShader = compile GLSL FS_COPY_GL4;
		ZWriteEnable = false;
	}

	context BRIGHTPASS
	{
		VertexShader = compile GLSL VS_FSQUAD_GL4;
		PixelShader = compile GLSL FS_BRIGHTPASS_GL4;
		
		ZWriteEnable = false;
	}

	context BLUR
	{
		VertexShader = compile GLSL VS_FSQUAD_GL4;
		PixelShader = compile GLSL FS_BLUR_GL4;
		
		ZWriteEnable = false;
	}

	context FINALPASS
	{
		VertexShader = compile GLSL VS_FSQUAD_GL4;
		PixelShader = compile GLSL FS_FINALPASS_GL4;
		
		ZWriteEnable = false;
	}
}

[[VS_FSQUAD]]
// =================================================================================================

uniform mat4 projMat;
attribute vec3 vertPos;
varying vec2 texCoords;
				
void main( void )
{
	texCoords = vertPos.xy; 
	gl_Position = projMat * vec4( vertPos, 1 );
}

[[VS_FSQUAD_GL4]]
// =================================================================================================

uniform mat4 projMat;

layout( location = 0 ) in vec3 vertPos;
out vec2 texCoords;
				
void main( void )
{
	texCoords = vertPos.xy; 
	gl_Position = projMat * vec4( vertPos, 1 );
}

[[FS_COPY]]
uniform sampler2D buf0;
varying vec2 texCoords;
void main() { gl_FragColor = texture2D(buf0, texCoords); }

[[FS_COPY_GL4]]
uniform sampler2D buf0;
in vec2 texCoords;
out vec4 fragColor;
void main() { fragColor = texture(buf0, texCoords); }


[[FS_BRIGHTPASS]]
// =================================================================================================

#include "shaders/utilityLib/fragPostProcess.glsl"

uniform sampler2D buf0;
uniform vec2 frameBufSize;
//uniform float hdrExposure;
uniform float hdrBrightThres;
uniform float hdrBrightOffset;
varying vec2 texCoords;

void main( void )
{
	vec2 texSize = frameBufSize * 4.0;
	vec2 coord2 = texCoords + vec2( 2, 2 ) / texSize;
	
	// Average using bilinear filtering
	vec4 sum = getTex2DBilinear( buf0, texCoords, texSize );
	sum += getTex2DBilinear( buf0, coord2, texSize );
	sum += getTex2DBilinear( buf0, vec2( coord2.x, texCoords.y ), texSize );
	sum += getTex2DBilinear( buf0, vec2( texCoords.x, coord2.y ), texSize );
	sum /= 4.0;
	
	// Tonemap
	//sum = 1.0 - exp2( -hdrExposure * sum );
	
	// Extract bright values
	sum = max( sum - hdrBrightThres, 0.0 );
	sum /= hdrBrightOffset + sum;
	
	gl_FragColor = sum;
}

[[FS_BRIGHTPASS_GL4]]
// =================================================================================================

#include "shaders/utilityLib/fragPostProcessGL4.glsl"

uniform sampler2D buf0;
uniform vec2 frameBufSize;
//uniform float hdrExposure;
uniform float hdrBrightThres;
uniform float hdrBrightOffset;
in vec2 texCoords;

out vec4 fragColor;

void main( void )
{
	vec2 texSize = frameBufSize * 4.0;
	vec2 coord2 = texCoords + vec2( 2, 2 ) / texSize;
	
	// Average using bilinear filtering
	vec4 sum = getTex2DBilinear( buf0, texCoords, texSize );
	sum += getTex2DBilinear( buf0, coord2, texSize );
	sum += getTex2DBilinear( buf0, vec2( coord2.x, texCoords.y ), texSize );
	sum += getTex2DBilinear( buf0, vec2( texCoords.x, coord2.y ), texSize );
	sum /= 4.0;
	
	// Tonemap
	//sum = 1.0 - exp2( -hdrExposure * sum );
	
	// Extract bright values
	sum = max( sum - hdrBrightThres, 0.0 );
	sum /= hdrBrightOffset + sum;
	
	fragColor = sum;
}

	
[[FS_BLUR]]
// =================================================================================================

#include "shaders/utilityLib/fragPostProcess.glsl"

uniform sampler2D buf0;
uniform vec2 frameBufSize;
uniform vec4 blurParams;
varying vec2 texCoords;

void main( void )
{
	gl_FragColor = blurKawase( buf0, texCoords, frameBufSize, blurParams.x );
}
	
[[FS_BLUR_GL4]]
// =================================================================================================

#include "shaders/utilityLib/fragPostProcessGL4.glsl"

uniform sampler2D buf0;
uniform vec2 frameBufSize;
uniform vec4 blurParams;
in vec2 texCoords;

out vec4 fragColor;

void main( void )
{
	fragColor = blurKawase( buf0, texCoords, frameBufSize, blurParams.x );
}


[[FS_FINALPASS]]
// =================================================================================================

uniform sampler2D buf0, buf1;
uniform vec2 frameBufSize;
uniform float hdrExposure;
uniform float hdrBloomStrength;
varying vec2 texCoords;

vec3 acesFilm( vec3 color )
{
	const float a = 2.51;
	const float b = 0.03;
	const float c = 2.43;
	const float d = 0.59;
	const float e = 0.14;
	return clamp( (color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0 );
}

void main( void )
{
	vec4 col0 = texture2D( buf0, texCoords );	// HDR color
	vec4 col1 = texture2D( buf1, texCoords );	// Bloom
	vec3 hdrColor = max( col0.rgb + col1.rgb * hdrBloomStrength, 0.0 ) * hdrExposure;
	vec3 displayColor = pow( acesFilm( hdrColor ), vec3( 1.0 / 2.2 ) );
	gl_FragColor = vec4( displayColor, col0.a );
}

[[FS_FINALPASS_GL4]]
// =================================================================================================

uniform sampler2D buf0, buf1;
uniform vec2 frameBufSize;
uniform float hdrExposure;
uniform float hdrBloomStrength;
in vec2 texCoords;

out vec4 fragColor;

vec3 acesFilm( vec3 color )
{
	const float a = 2.51;
	const float b = 0.03;
	const float c = 2.43;
	const float d = 0.59;
	const float e = 0.14;
	return clamp( (color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0 );
}

void main( void )
{
	vec4 col0 = texture( buf0, texCoords );	// HDR color
	vec4 col1 = texture( buf1, texCoords );	// Bloom
	vec3 hdrColor = max( col0.rgb + col1.rgb * hdrBloomStrength, 0.0 ) * hdrExposure;
	vec3 displayColor = pow( acesFilm( hdrColor ), vec3( 1.0 / 2.2 ) );
	fragColor = vec4( displayColor, col0.a );
}
