uniform float pbrDebugView;
uniform vec4 baseColorTexTransform;
uniform vec4 metalRoughTexTransform;
uniform vec4 normalTexTransform;
uniform vec4 occlusionTexTransform;
uniform vec4 emissiveTexTransform;
uniform float baseColorTexRotation;
uniform float metalRoughTexRotation;
uniform float normalTexRotation;
uniform float occlusionTexRotation;
uniform float emissiveTexRotation;
uniform float baseIor;
uniform vec4 iridescenceParams;
uniform sampler2D iridescenceMap;
uniform sampler2D iridescenceThicknessMap;
uniform vec4 iridescenceTexTransform;
uniform vec4 iridescenceThicknessTexTransform;
uniform float iridescenceTexRotation;
uniform float iridescenceThicknessTexRotation;
uniform vec4 transmissionParams;
uniform vec4 attenuationColor;
uniform sampler2D transmissionMap;
uniform sampler2D volumeThicknessMap;
uniform vec4 transmissionTexTransform;
uniform vec4 volumeThicknessTexTransform;
uniform float transmissionTexRotation;
uniform float volumeThicknessTexRotation;
uniform sampler2D sceneColor;
uniform vec2 frameBufSize;
uniform vec4 clearcoatParams;
uniform vec4 anisotropyParams;

vec2 gltfTextureCoord(vec4 transform, float rotation)
{
	vec2 scaled = texCoord * transform.zw;
	float cosine = cos(rotation), sine = sin(rotation);
	return transform.xy + vec2(cosine * scaled.x - sine * scaled.y,
		sine * scaled.x + cosine * scaled.y);
}

vec3 gltfEvalSensitivity(float opticalPathDifference, vec3 shift)
{
	const float pi = 3.14159265359;
	float phase = 2.0 * pi * opticalPathDifference * 1.0e-9;
	vec3 value = vec3(5.4856e-13, 4.4201e-13, 5.2481e-13);
	vec3 position = vec3(1.6810e6, 1.7953e6, 2.2084e6);
	vec3 variance = vec3(4.3278e9, 9.3046e9, 6.6121e9);
	vec3 xyz = value * sqrt(2.0 * pi * variance) * cos(position * phase + shift) *
		exp(-(phase * phase) * variance);
	xyz.x += 9.7470e-14 * sqrt(2.0 * pi * 4.5282e9) *
		cos(2.2399e6 * phase + shift.x) * exp(-4.5282e9 * phase * phase);
	xyz /= 1.0685e-7;
	const mat3 xyzToRec709 = mat3(
		3.2404542, -0.9692660, 0.0556434,
		-1.5371385, 1.8760108, -0.2040259,
		-0.4985314, 0.0415560, 1.0572252);
	return xyzToRec709 * xyz;
}

vec3 gltfIridescentFresnel(float outsideIor, float filmIor, vec3 baseF0,
	float thickness, float cosineTheta1)
{
	const float pi = 3.14159265359;
	float ratio = outsideIor / filmIor;
	float cosineTheta2Squared = 1.0 - ratio * ratio * (1.0 - cosineTheta1 * cosineTheta1);
	if(cosineTheta2Squared < 0.0) return vec3(1.0);
	float cosineTheta2 = sqrt(cosineTheta2Squared);
	float interfaceF0 = pow((filmIor - outsideIor) / (filmIor + outsideIor), 2.0);
	float r12 = interfaceF0 + (1.0 - interfaceF0) * pow(1.0 - cosineTheta1, 5.0);
	float transmission121 = 1.0 - r12;
	vec3 sqrtBaseF0 = sqrt(clamp(baseF0, vec3(0.0), vec3(0.9999)));
	vec3 substrateIor = (vec3(1.0) + sqrtBaseF0) / (vec3(1.0) - sqrtBaseF0);
	vec3 interface23F0 = pow((substrateIor - filmIor) / (substrateIor + filmIor), vec3(2.0));
	vec3 r23 = interface23F0 + (vec3(1.0) - interface23F0) * pow(1.0 - cosineTheta2, 5.0);
	float phi12 = filmIor < outsideIor ? pi : 0.0;
	vec3 phi23 = step(substrateIor, vec3(filmIor)) * pi;
	vec3 phaseShift = vec3(pi - phi12) + phi23;
	float opticalPathDifference = 2.0 * filmIor * thickness * cosineTheta2;
	vec3 r123 = clamp(r12 * r23, vec3(1.0e-5), vec3(0.9999));
	vec3 rootR123 = sqrt(r123);
	vec3 series = transmission121 * transmission121 * r23 / (vec3(1.0) - r123);
	vec3 result = vec3(r12) + series;
	vec3 coefficient = series - vec3(transmission121);
	for(int order = 1; order <= 2; ++order)
	{
		coefficient *= rootR123;
		result += coefficient * 2.0 * gltfEvalSensitivity(float(order) * opticalPathDifference,
			float(order) * phaseShift);
	}
	return max(result, vec3(0.0));
}

vec3 gltfEnhanceDielectricIridescence(vec3 fresnel, float metallic)
{
	// Thin-film colors on a bright dielectric base can be lost after tone mapping.
	// Preserve reflected luminance while modestly separating the interference hues.
	float luminance = dot(fresnel, vec3(0.2126, 0.7152, 0.0722));
	vec3 enhanced = clamp(vec3(luminance) + (fresnel - vec3(luminance)) * 1.45,
		vec3(0.0), vec3(1.0));
	return mix(enhanced, fresnel, metallic);
}

float gltfIridescenceStrength()
{
	if(iridescenceParams.x <= 0.0) return 0.0;
	return clamp(iridescenceParams.x * GLTF_TEX(iridescenceMap,
		gltfTextureCoord(iridescenceTexTransform, iridescenceTexRotation)).r, 0.0, 1.0);
}

float gltfIridescenceThickness()
{
	float sampleValue = GLTF_TEX(iridescenceThicknessMap,
		gltfTextureCoord(iridescenceThicknessTexTransform, iridescenceThicknessTexRotation)).g;
	return mix(iridescenceParams.z, iridescenceParams.w, sampleValue);
}

float gltfTransmissionStrength()
{
	if(transmissionParams.x <= 0.0) return 0.0;
	return clamp(transmissionParams.x * GLTF_TEX(transmissionMap,
		gltfTextureCoord(transmissionTexTransform, transmissionTexRotation)).r, 0.0, 1.0);
}

vec3 gltfVolumeAttenuation(vec3 rayDirection)
{
	if(transmissionParams.y <= 0.0 || transmissionParams.z >= 1.0e19) return vec3(1.0);
	float thickness = transmissionParams.y * GLTF_TEX(volumeThicknessMap,
		gltfTextureCoord(volumeThicknessTexTransform, volumeThicknessTexRotation)).g;
	if(thickness <= 0.0) return vec3(1.0);
	float pathLength = thickness / max(abs(dot(normalize(tangentToWorld[2]), rayDirection)), 0.1);
	return pow(clamp(attenuationColor.rgb, vec3(1.0e-6), vec3(1.0)),
		vec3(pathLength / max(transmissionParams.z, 1.0e-6)));
}

vec3 gltfSampleTransmittedScene(float roughness)
{
	vec2 uv = gl_FragCoord.xy / frameBufSize;
	float lod = clamp(roughness, 0.0, 1.0) * 7.0;
	return GLTF_TEX_LOD(sceneColor, uv, lod).rgb;
}

vec3 gltfFresnelRoughness(float cosine, vec3 f0, float roughness)
{
	return f0 + (max(vec3(1.0 - roughness), f0) - f0) * pow(1.0 - cosine, 5.0);
}

void gltfAnisotropyBasis(vec3 normal, out vec3 anisotropicT, out vec3 anisotropicB, out float strength)
{
	strength = clamp(anisotropyParams.x, 0.0, 1.0);
	float rotation = anisotropyParams.y;
	vec2 direction = vec2(cos(rotation), sin(rotation));
	anisotropicT = normalize(tangentToWorld * vec3(direction, 0.0));
	anisotropicB = normalize(cross(normal, anisotropicT));
}

vec3 gltfSrgbToLinear(vec3 color)
{
	vec3 low = color / 12.92;
	vec3 high = pow((color + 0.055) / 1.055, vec3(2.4));
	return mix(low, high, step(vec3(0.04045), color));
}

vec4 gltfBaseColor()
{
	vec4 sampled = GLTF_TEX(baseColorMap, gltfTextureCoord(baseColorTexTransform, baseColorTexRotation));
	return vec4(gltfSrgbToLinear(sampled.rgb) * baseColorFactor.rgb,
		sampled.a * baseColorFactor.a);
}

void gltfMaterial(out vec4 color, out float metallic, out float roughness, out vec3 normal)
{
	color = gltfBaseColor();
	vec4 metalRough = GLTF_TEX(metallicRoughnessMap, gltfTextureCoord(metalRoughTexTransform, metalRoughTexRotation));
	metallic = clamp(metalRough.b * pbrParams.x, 0.0, 1.0);
	roughness = clamp(metalRough.g * pbrParams.y, 0.045, 1.0);
	vec3 tangentNormal = vec3(0.0, 0.0, 1.0);
	if(normalScale != 0.0)
	{
		tangentNormal = GLTF_TEX(normalMap,
			gltfTextureCoord(normalTexTransform, normalTexRotation)).xyz * 2.0 - 1.0;
		tangentNormal.xy *= normalScale;
	}
	normal = normalize(tangentToWorld * normalize(tangentNormal));
	// Increase roughness only where the normal field changes faster than a pixel.
	// This is geometric/specular AA: it keeps authored detail but prevents tiny
	// normal-map features from turning HDR highlights into colored sparkles.
	vec3 normalDx = dFdx(normal);
	vec3 normalDy = dFdy(normal);
	float normalVariance = 0.5 * (dot(normalDx, normalDx) + dot(normalDy, normalDy));
	roughness = clamp(sqrt(roughness * roughness + min(normalVariance, 0.25)), 0.06, 1.0);
}

vec3 gltfSamplePrefilter(vec3 direction, float roughness)
{
	float value = clamp(roughness, 0.0, 1.0);
	if(value < 0.25) return mix(GLTF_CUBE(iblSpecular0, direction).rgb,
		GLTF_CUBE(iblSpecular1, direction).rgb, value * 4.0);
	if(value < 0.5) return mix(GLTF_CUBE(iblSpecular1, direction).rgb,
		GLTF_CUBE(iblSpecular2, direction).rgb, (value - 0.25) * 4.0);
	return mix(GLTF_CUBE(iblSpecular2, direction).rgb,
		GLTF_CUBE(iblSpecular3, direction).rgb, (value - 0.5) * 2.0);
}

vec3 gltfSampleAnisotropicPrefilter(vec3 normal, vec3 view, vec3 anisotropicB,
	float roughness, float strength)
{
	// KHR_materials_anisotropy recommends the center of the anisotropic
	// reflection distribution as the real-time single-sample PMREM direction.
	// cross(cross(B, V), B) is V projected off the anisotropic bitangent.
	vec3 projectedView = view - anisotropicB * dot(anisotropicB, view);
	float projectedLength = length(projectedView);
	vec3 projectedNormal = projectedView * inversesqrt(max(projectedLength * projectedLength, 1.0e-6));
	// The projection is undefined when V is parallel to B. Fade to the surface
	// normal around that pole to prevent a pointed discontinuity on smooth meshes.
	projectedNormal = normalize(mix(normal, projectedNormal,
		smoothstep(0.04, 0.20, projectedLength)));
	float bend = 1.0 - strength * (1.0 - roughness);
	bend *= bend;
	bend *= bend;
	vec3 bentNormal = normalize(mix(projectedNormal, normal, bend));
	vec3 reflection = reflect(-view, bentNormal);
	reflection = normalize(mix(reflection, bentNormal, roughness * roughness));
	return gltfSamplePrefilter(reflection, roughness);
}

vec4 shadeGltfPbr()
{
	vec4 color;
	float metallic, roughness;
	vec3 normal;
	gltfMaterial(color, metallic, roughness, normal);
	float ao = 1.0;
	if(pbrParams.z > 0.0)
		ao = mix(1.0, GLTF_TEX(occlusionMap,
			gltfTextureCoord(occlusionTexTransform, occlusionTexRotation)).r,
			clamp(pbrParams.z, 0.0, 1.0));
	if(pbrDebugView > 0.5 && pbrDebugView < 1.5) return vec4(vec3(metallic), 1.0);
	if(pbrDebugView > 1.5 && pbrDebugView < 2.5) return vec4(vec3(roughness), 1.0);
	if(pbrDebugView > 2.5 && pbrDebugView < 3.5) return vec4(normal * 0.5 + 0.5, 1.0);
	if(pbrDebugView > 3.5) return vec4(vec3(ao), 1.0);
	vec3 view = normalize(viewerPos - worldPosition);
	float nDotV = max(dot(normal, view), 0.0);
	float dielectricF0 = pow((baseIor - 1.0) / (baseIor + 1.0), 2.0);
	vec3 f0 = mix(vec3(dielectricF0), color.rgb, metallic);
	vec3 baseFresnel = gltfFresnelRoughness(nDotV, f0, roughness);
	float iridescenceStrength = gltfIridescenceStrength();
	vec3 iridescentFresnel = baseFresnel;
	if(iridescenceStrength > 0.0)
		iridescentFresnel = gltfIridescentFresnel(1.0, iridescenceParams.y, f0,
			gltfIridescenceThickness(), nDotV);
	iridescentFresnel = gltfEnhanceDielectricIridescence(iridescentFresnel, metallic);
	vec3 fresnel = mix(baseFresnel, iridescentFresnel, iridescenceStrength);
	// The irradiance cubemap stores the cosine-weighted hemisphere integral.
	// Lambert's BRDF contributes the matching 1 / PI normalization.
	vec3 diffuse = GLTF_CUBE(iblIrradiance, normal).rgb * color.rgb / 3.14159265359;
	vec3 anisotropicT, anisotropicB;
	float anisotropyStrength;
	gltfAnisotropyBasis(normal, anisotropicT, anisotropicB, anisotropyStrength);
	vec3 reflected = gltfSampleAnisotropicPrefilter(normal, view, anisotropicB,
		roughness, anisotropyStrength);
	vec2 brdf = GLTF_TEX(iblBrdfLut, vec2(nDotV, roughness)).rg;
	vec3 specular = reflected * (fresnel * brdf.x + brdf.y);
	vec3 baseKd = (vec3(1.0) - baseFresnel) * (1.0 - metallic);
	vec3 iridescentKd = vec3(1.0 - max(max(iridescentFresnel.r, iridescentFresnel.g), iridescentFresnel.b)) * (1.0 - metallic);
	vec3 kd = mix(baseKd, iridescentKd, iridescenceStrength);
	float transmissionStrength = gltfTransmissionStrength() * (1.0 - metallic);
	vec3 transmitted = diffuse;
	if(transmissionStrength > 0.0)
	{
		vec3 refractedDirection = refract(-view, normal, 1.0 / max(baseIor, 1.0));
		if(dot(refractedDirection, refractedDirection) < 1.0e-6)
			refractedDirection = reflect(-view, normal);
		transmitted = gltfSampleTransmittedScene(roughness) * color.rgb *
			gltfVolumeAttenuation(normalize(refractedDirection));
	}
	vec3 baseLighting = kd * mix(diffuse, transmitted, transmissionStrength) + specular;
	float clearcoatFactor = clamp(clearcoatParams.x, 0.0, 1.0);
	if(clearcoatFactor > 0.0)
	{
		float clearcoatRoughness = clamp(clearcoatParams.y, 0.045, 1.0);
		float clearcoatFresnel = 0.04 + 0.96 * pow(1.0 - nDotV, 5.0);
		vec3 clearcoatReflected = gltfSamplePrefilter(reflect(-view, normal), clearcoatRoughness);
		vec2 clearcoatBrdf = GLTF_TEX(iblBrdfLut, vec2(nDotV, clearcoatRoughness)).rg;
		vec3 clearcoatSpecular = clearcoatReflected *
			(clearcoatFresnel * clearcoatBrdf.x + clearcoatBrdf.y);
		baseLighting = baseLighting * (1.0 - clearcoatFactor * clearcoatFresnel) +
			clearcoatSpecular * clearcoatFactor;
	}
	vec3 emissive = vec3(0.0);
	if(emissiveFactor.a > 0.0 && dot(emissiveFactor.rgb, emissiveFactor.rgb) > 0.0)
		emissive = gltfSrgbToLinear(GLTF_TEX(emissiveMap,
			gltfTextureCoord(emissiveTexTransform, emissiveTexRotation)).rgb) *
			emissiveFactor.rgb * emissiveFactor.a;
	float surfaceAlpha = mix(color.a, 1.0, transmissionStrength);
	return vec4((baseLighting * ao * pbrParams.w) + emissive, surfaceAlpha);
}
