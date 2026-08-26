const float GLTF_PI = 3.14159265359;

float gltfDistributionGGX(vec3 normal, vec3 halfVector, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float nDotH = max(dot(normal, halfVector), 0.0);
	float denominator = nDotH * nDotH * (a2 - 1.0) + 1.0;
	return a2 / max(GLTF_PI * denominator * denominator, 0.000001);
}

float gltfGeometrySchlickGGX(float nDotDirection, float roughness)
{
	float r = roughness + 1.0;
	float k = r * r / 8.0;
	return nDotDirection / max(nDotDirection * (1.0 - k) + k, 0.000001);
}

float gltfGeometrySmith(vec3 normal, vec3 view, vec3 light, float roughness)
{
	return gltfGeometrySchlickGGX(max(dot(normal, view), 0.0), roughness) *
		gltfGeometrySchlickGGX(max(dot(normal, light), 0.0), roughness);
}

float gltfDistributionGGXAnisotropic(float nDotH, float tDotH, float bDotH,
	float alphaT, float alphaB)
{
	float alpha2 = alphaT * alphaB;
	vec3 f = vec3(alphaB * tDotH, alphaT * bDotH, alpha2 * nDotH);
	float w2 = alpha2 / max(dot(f, f), 0.000001);
	return alpha2 * w2 * w2 / GLTF_PI;
}

float gltfVisibilityGGXAnisotropic(float nDotL, float nDotV,
	float bDotV, float tDotV, float tDotL, float bDotL, float alphaT, float alphaB)
{
	float ggxV = nDotL * length(vec3(alphaT * tDotV, alphaB * bDotV, nDotV));
	float ggxL = nDotV * length(vec3(alphaT * tDotL, alphaB * bDotL, nDotL));
	return clamp(0.5 / max(ggxV + ggxL, 0.000001), 0.0, 1.0);
}

vec3 gltfFresnel(float cosine, vec3 f0)
{
	return f0 + (1.0 - f0) * pow(clamp(1.0 - cosine, 0.0, 1.0), 5.0);
}

vec3 shadeGltfPbrLight(float viewDistance)
{
	if(pbrDebugView > 0.5) return vec3(0.0);
	vec4 color;
	float metallic, roughness;
	vec3 normal;
	gltfMaterial(color, metallic, roughness, normal);

	vec3 toLight = lightPos.xyz - worldPosition;
	float lightDistance = length(toLight);
	vec3 light = toLight / max(lightDistance, 0.000001);
	vec3 view = normalize(viewerPos - worldPosition);
	vec3 halfVector = normalize(view + light);
	float nDotL = max(dot(normal, light), 0.0);
	float nDotV = max(dot(normal, view), 0.0);

	float lightDepth = lightDistance / lightPos.w;
	float attenuation = max(1.0 - lightDepth * lightDepth, 0.0);
	float angle = dot(lightDir.xyz, -light);
	attenuation *= clamp((angle - lightDir.w) / 0.2, 0.0, 1.0);

	float dielectricF0 = pow((baseIor - 1.0) / (baseIor + 1.0), 2.0);
	vec3 f0 = mix(vec3(dielectricF0), color.rgb, metallic);
	float viewHalf = max(dot(halfVector, view), 0.0);
	vec3 baseFresnel = gltfFresnel(viewHalf, f0);
	float iridescenceStrength = gltfIridescenceStrength();
	vec3 iridescentFresnel = baseFresnel;
	if(iridescenceStrength > 0.0)
		iridescentFresnel = gltfIridescentFresnel(1.0, iridescenceParams.y, f0,
			gltfIridescenceThickness(), viewHalf);
	iridescentFresnel = gltfEnhanceDielectricIridescence(iridescentFresnel, metallic);
	vec3 fresnel = mix(baseFresnel, iridescentFresnel, iridescenceStrength);
	vec3 anisotropicT, anisotropicB;
	float anisotropyStrength;
	gltfAnisotropyBasis(normal, anisotropicT, anisotropicB, anisotropyStrength);
	float alphaRoughness = roughness * roughness;
	float alphaT = mix(alphaRoughness, 1.0, anisotropyStrength * anisotropyStrength);
	float alphaB = alphaRoughness;
	float distribution = gltfDistributionGGXAnisotropic(max(dot(normal, halfVector), 0.0),
		dot(anisotropicT, halfVector), dot(anisotropicB, halfVector), alphaT, alphaB);
	float visibility = gltfVisibilityGGXAnisotropic(nDotL, nDotV,
		dot(anisotropicB, view), dot(anisotropicT, view), dot(anisotropicT, light),
		dot(anisotropicB, light), alphaT, alphaB);
	vec3 specular = distribution * visibility * fresnel;
	vec3 baseKd = (vec3(1.0) - baseFresnel) * (1.0 - metallic);
	vec3 iridescentKd = vec3(1.0 - max(max(iridescentFresnel.r, iridescentFresnel.g), iridescentFresnel.b)) * (1.0 - metallic);
	vec3 kd = mix(baseKd, iridescentKd, iridescenceStrength);

	float shadowTerm = 1.0;
	if(attenuation * (shadowMapSize - 4.0) > 0.0)
	{
		vec4 projected = shadowMats[3] * vec4(worldPosition, 1.0);
		if(viewDistance < shadowSplitDists.x) projected = shadowMats[0] * vec4(worldPosition, 1.0);
		else if(viewDistance < shadowSplitDists.y) projected = shadowMats[1] * vec4(worldPosition, 1.0);
		else if(viewDistance < shadowSplitDists.z) projected = shadowMats[2] * vec4(worldPosition, 1.0);
		projected.z = lightDepth;
		projected.xy /= projected.w;
		shadowTerm = max(PCF(projected), 0.03);
	}

	float opaqueDiffuse = 1.0 - gltfTransmissionStrength() * (1.0 - metallic);
	vec3 baseBrdf = kd * color.rgb / GLTF_PI * opaqueDiffuse + specular;
	float clearcoatFactor = clamp(clearcoatParams.x, 0.0, 1.0);
	float clearcoatRoughness = clamp(clearcoatParams.y, 0.045, 1.0);
	float clearcoatFresnel = 0.04 + 0.96 * pow(1.0 - viewHalf, 5.0);
	float clearcoatDistribution = gltfDistributionGGX(normal, halfVector, clearcoatRoughness);
	float clearcoatGeometry = gltfGeometrySmith(normal, view, light, clearcoatRoughness);
	float clearcoatSpecular = clearcoatDistribution * clearcoatGeometry * clearcoatFresnel /
		max(4.0 * nDotV * nDotL, 0.0001);
	vec3 layeredBrdf = baseBrdf * (1.0 - clearcoatFactor * clearcoatFresnel) +
		vec3(clearcoatSpecular) * clearcoatFactor;
	return layeredBrdf * lightColor *
		attenuation * nDotL * shadowTerm;
}
