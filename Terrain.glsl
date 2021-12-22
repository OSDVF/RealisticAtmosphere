//?#version 440
#include "Random.glsl"
#include "Buffers.glsl"

float getSampleParameters(Planet planet, Ray ray, float currentDistance, out vec3 sphNormal, out vec3 worldSamplePos);

#ifdef COMPUTE
uvec2 quadIndex = gl_LocalInvocationID.xy / 2;
shared vec3 hitPosition;
float mip_map_level(in vec2 texture_coordinate) // in texel units
{
return 1;
	/*vec2  dx_vtc = variable[quadIndex.x + 1][quadIndex.y + 0] - variable[quadIndex.x + 0][quadIndex.y + 0];
	vec2  dy_vtc = variable[quadIndex.x + 0][quadIndex.y + 1] - variable[quadIndex.x + 0][quadIndex.y + 0];
    vec2  dx_vtc        = dFdx(texture_coordinate);
    vec2  dy_vtc        = dFdy(texture_coordinate);
    float delta_max_sqr = max(dot(dx_vtc, dx_vtc), dot(dy_vtc, dy_vtc));
    float mml = 0.5 * log2(delta_max_sqr);
    return max( 0, mml ); // Thanks @Nims*/
}
#else
float mip_map_level(in vec2 texture_coordinate) // in texel units
{
    vec2  dx_vtc        = dFdx(texture_coordinate);
    vec2  dy_vtc        = dFdy(texture_coordinate);
    float delta_max_sqr = max(dot(dx_vtc, dx_vtc), dot(dy_vtc, dy_vtc));
    float mml = 0.5 * log2(delta_max_sqr);
    return max( 0, mml ); // Thanks @Nims
}
#endif
vec3 triplanarSample(sampler2D sampl, vec3 pos, vec3 normal)
{
	vec2 triUVx = (pos.zy);
	vec2 triUVy = (pos.xz);
	vec2 triUVz = (pos.xy);
	#ifdef COMPUTE
	vec4 texX = textureLod(sampl, triUVx, mip_map_level(pos.zy));
	vec4 texY = textureLod(sampl, triUVy, mip_map_level(pos.xz));
	vec4 texZ = textureLod(sampl, triUVz, mip_map_level(pos.xy));
	#else
	const float bigUvFrac = 100;
	vec2 biggerUV = triUVx/bigUvFrac;
	vec4 texX = textureGrad(sampl, triUVx, dFdx(triUVx),dFdy(triUVx));
	texX *= textureGrad(sampl, biggerUV, dFdx(biggerUV),dFdy(biggerUV));
	vec4 texY = textureGrad(sampl, triUVy, dFdx(triUVy),dFdy(triUVy));
	biggerUV = triUVy/bigUvFrac;
	texY *= textureGrad(sampl, biggerUV, dFdx(biggerUV),dFdy(biggerUV));
	vec4 texZ = textureGrad(sampl, triUVz, dFdx(triUVz),dFdy(triUVz));
	biggerUV = triUVz/bigUvFrac;
	texZ *= textureGrad(sampl, biggerUV, dFdx(biggerUV),dFdy(biggerUV));
	#endif

	vec3 weights = abs(normal);
	weights /= (weights.x + weights.y + weights.z);

	vec4 color = texX * weights.x + texY * weights.y + texZ * weights.z;
	return color.xyz;
}

vec3 terrainColor(Planet planet, vec3 camPos, vec3 pos, vec3 normal)
{
	// Triplanar texture mapping in world space
	float distFromSurface = distance(pos, planet.center) - planet.surfaceRadius;
	float elev = terrainElevation(pos);
	float gradHeight = 5000 * elev;
	float randomStrength = 10000;
	distFromSurface -= randomStrength * elev;
	if(distFromSurface < PlanetMaterial.x)
	{
		return triplanarSample(texSampler1, pos, normal);
	}
	else if(distFromSurface < PlanetMaterial.x+gradHeight)
	{
		return mix(triplanarSample(texSampler1, pos, normal),triplanarSample(texSampler2, pos, normal),
					smoothstep(0,gradHeight,distFromSurface-PlanetMaterial.x));
	}
	else if(distFromSurface < PlanetMaterial.y)
	{
		return triplanarSample(texSampler2, pos, normal);
	}
	else if(distFromSurface < PlanetMaterial.y+gradHeight)
	{
		return mix(triplanarSample(texSampler2, pos, normal),triplanarSample(texSampler3, pos, normal),
					smoothstep(0,gradHeight,distFromSurface-PlanetMaterial.y));
	}
	else
	{
		return triplanarSample(texSampler3, pos, normal);
	}
}

float pow3(float f) {
    return f * f * f;
}

vec3 biLerp(vec3 a, vec3 b, vec3 c, vec3 d, float s, float t)
{
  vec3 x = mix(a, b, t);
  vec3 y = mix(c, d, t);
  return mix(x, y, s);
}

vec3 terrainNormal(vec2 normalMap, vec3 sphNormal)
{
	// Compute normal in world space
	vec2 map = (normalMap) * 2 - 1;
	vec3 t = sphereTangent(sphNormal);
	vec3 bitangent = sphNormal * t;
	float normalZ = sqrt(1-dot(map.xy, map.xy));
	return normalize(map.x * t + map.y * bitangent + normalZ * sphNormal);
}

float terrainSDF(Planet planet, float sampleHeight /*above sea level*/, vec3 sphNormal, out vec2 outNormalMap)
{
	vec3 bump;
	#if 1
	// Perform hermite bilinear interpolation of texture
	ivec2 mapSize = textureSize(heightmapTexture,0);
	vec2 uvScaled = toUV(sphNormal) * mapSize;
	ivec2 coords = ivec2(uvScaled);
	vec2 coordDiff = uvScaled - coords;
	vec3 bump1 = texelFetch(heightmapTexture, coords, 0).xyz;
	vec3 bump2 = texelFetch(heightmapTexture, coords+ivec2(1,0), 0).xyz;
	vec3 bump3 = texelFetch(heightmapTexture, coords+ivec2(0,1), 0).xyz;
	vec3 bump4 = texelFetch(heightmapTexture, coords+ivec2(1,1), 0).xyz;
	vec2 sm = smoothstep(0,1,coordDiff);

	// Result interpolated texture
	bump = biLerp(bump1,bump2,bump3,bump4,sm.y,sm.x);
	#else
	bump = texture(heightmapTexture,toUV(sphNormal)).xyz;
	#endif

	float mountainHeight = planet.mountainsRadius -  planet.surfaceRadius;
	float surfaceHeight = bump.x * mountainHeight;
	outNormalMap = bump.gb;
	return sampleHeight - surfaceHeight;
}

float getSampleParameters(Planet planet, Ray ray, float currentDistance, out vec3 sphNormal, out vec3 worldSamplePos)
{
	worldSamplePos = ray.origin + currentDistance * ray.direction;
	vec3 centerToSample = worldSamplePos - planet.center;
	sphNormal = normalize(centerToSample);
	float centerDist = length(centerToSample);
	// Height above the sea level
	return centerDist - planet.surfaceRadius;
}

/**
  * @returns world-space position of intersection
  */
vec3 bisectTerrain(Planet planet, Ray ray, float fromT, inout float toT, out vec2 outNormalMap, out vec3 sphNormal)
{
	for(int i = 0; i < QualitySettings_steps; i++)
	{
		float currentT = (toT - fromT)/2;
		vec3 worldSamplePos;
		float sampleHeight = getSampleParameters(planet, ray, currentT, sphNormal, worldSamplePos);

		float terrainDistance = terrainSDF(planet, sampleHeight, sphNormal, outNormalMap);
		if(abs(terrainDistance) < QualitySettings_precision)
		{
			return worldSamplePos;
		}
		if(terrainDistance < 0)
		{
			toT = currentT;
		}
		else
		{
			fromT = currentT;
		}
	}
	return vec3(0);
}