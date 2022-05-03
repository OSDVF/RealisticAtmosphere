//?#version 440
#ifndef TERRAIN_H_
#define TERRAIN_H_
#include "Random.glsl"
#include "Buffers.glsl"
#include "Intersections.glsl"

float getSampleParameters(Planet planet, Ray ray, float currentDistance, out vec3 sphNormal, out vec3 worldSamplePos);
vec2 mirrorTilingUV(vec2 uv);

vec3 triplanarSample(sampler2DArray sampl, vec4 pos, vec3 normal, float lod)
{
	pos.xyz = pos.xyz / 5;
	vec3 triUVx = vec3(pos.zy,pos.w);
	vec3 triUVy = vec3(pos.xz,pos.w);
	vec3 triUVz = vec3(pos.xy,pos.w);
	float bigLod = lod-0.5;
	float bigUvFrac = 10;
	vec3 biggerUV = triUVx;
	biggerUV.xy /= bigUvFrac;
	vec4 texX = textureLod(sampl, triUVx, lod);
	texX *= textureLod(sampl, biggerUV, bigLod);

	vec4 texY = textureLod(sampl, triUVy, lod);
	biggerUV.xy = triUVy.xy/bigUvFrac;
	texY *= textureLod(sampl, biggerUV, bigLod);

	vec4 texZ = textureLod(sampl, triUVz, lod);
	biggerUV.xy = triUVz.xy/bigUvFrac;
	texZ *= textureLod(sampl, biggerUV, bigLod);

	vec3 weights = abs(normal);
	weights /= (weights.x + weights.y + weights.z);

	vec4 color = texX * weights.x + texY * weights.y + texZ * weights.z;
	return color.xyz;
}

float terrainCoverage(vec2 uv)
{
	return clamp(
		pow(
			(
				Value2D(uv*0.0000002)*.5 +//basically inlined fBm
				Value2D(uv*0.0000004)*.25 +
				Value2D(uv*0.0000008)*.125 +
				Value2D(uv*0.0000016)*.625 +
				Value2D(uv*0.0000032)*.3125 +
				Value2D(uv*0.0000064)*.15625
			)*
			2-1.1,//make more sea than land
			11.0),//sharpen the shores
		-0.1,1.0);
}

vec3 terrainColor(Planet planet, float T, vec3 pos, vec3 normal, float elev)
{
	// Triplanar texture mapping in world space
	float lod = pow(T, RaymarchingSteps.w) / QualitySettings_lodPow;
	vec4 gradParams = texture(heightmapTexture, mirrorTilingUV(pos.xz*5+100));
	float gradHeight = PlanetMaterial.w;
	float randomizedElev = elev * (gradParams.x+gradParams.w);
	float firstRatio = clamp((elev - PlanetMaterial.x) / PlanetMaterial.y,0,1);
	return mix(
				mix(
					mix(
						triplanarSample(terrainTextures, vec4(pos,2), normal, lod),
						triplanarSample(terrainTextures, vec4(pos,1), normal, lod),
						pow(mix(firstRatio, gradParams.y, gradParams.z*PlanetMaterial.z),2)) + smoothstep(PlanetMaterial.x,PlanetMaterial.y,randomizedElev-800)*0.6,
					triplanarSample(terrainTextures, vec4(pos,0), normal, lod),
						clamp(pow((gradParams.y+gradParams.z+gradParams.w-PlanetMaterial.w),10), 0,1)
				),
			triplanarSample(terrainTextures, vec4(pos,3), normal, lod),
			max(clamp(pow(-(normal.x+normal.z)*7,15),0,1),1 - smoothstep(700,1000,randomizedElev))
		) * terrainCoverage(pos.xz);
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
	vec2 map = (normalMap * 0.9) * 2 - 1;
	map.x = -map.x;
	vec3 t = vec3(1,0,0);//sphereTangent(sphNormal);
	vec3 bitangent = vec3(0,0,1);//sphNormal * t;
	float normalZ = sqrt(1-dot(map.xy, map.xy));
	return normalize(map.x * t + map.y * bitangent + normalZ * vec3(0,1,0));
}

vec2 planetUV(vec2 uv)
{
	return mod(uv*0.00003, 1);
}
vec2 mirrorTilingUV(vec2 uv)
{
	vec2 scaled = mod(uv*0.00003,2);
	if(scaled.x > 1)
	{
		scaled.x = 1 - (scaled.x - 1);
	}
	if(scaled.y > 1)
	{
		scaled.y = 1 - (scaled.y - 1);
	}
	return scaled;
}

float terrainSDF(Planet planet, float sampleHeight /*above sea level*/, vec2 uv, out vec2 outNormalMap)
{
	vec3 bump;
	#if 0
	// Perform hermite bilinear interpolation of texture
	ivec2 mapSize = textureSize(heightmapTexture,0);
	vec2 uvScaled = planetUV(uv) * mapSize;
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
	float coverage = terrainCoverage(uv);
	bump = texture(heightmapTexture, planetUV(uv)).xyz;
	bump.x *= coverage;
	bump.yz = mix(vec2(0.5), bump.yz, coverage);
	#endif

	double mountainHeight = double(planet.mountainsRadius) -  double(planet.surfaceRadius);
	float surfaceHeight = bump.x * float(mountainHeight);
	outNormalMap = bump.gb;
	return float(sampleHeight - surfaceHeight);
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

float getSampleParametersH(Planet planet, Ray ray, float currentDistance, out vec3 sphNormal, out vec3 worldSamplePos)
{
	dvec3 worldSamplePosH = dvec3(ray.origin) + double(currentDistance) * dvec3(ray.direction);
	dvec3 centerToSample = worldSamplePosH - dvec3(planet.center);
	sphNormal = normalize(vec3(centerToSample));
	double centerDist = length(centerToSample);
	// Height above the sea level
	worldSamplePos = vec3(worldSamplePosH);
	return float(centerDist - double(planet.surfaceRadius));
}

bool raymarchTerrain(Planet planet, Ray ray, float fromDistance, inout float toDistance, out vec2 normalMap, out vec3 sphNormal, out vec3 worldSamplePos, out float sampleHeight)
{
	float t0, t1;
	if(!(raySphereIntersection(planet.center, planet.mountainsRadius, ray, t0, t1)&&t1>0))
		return false;

	fromDistance = max(fromDistance, t0);

	float currentT = fromDistance;
	float maxDistance = min(toDistance, QualitySettings_farPlane);
	float terrainDistance = POSITIVE_INFINITY;

	for(int i = 0; i < floatBitsToInt(RaymarchingSteps.x);i++)
	{
		if(currentT <= maxDistance)
		{
			sampleHeight = getSampleParametersH(planet, ray, currentT, /*out*/sphNormal, /*out*/worldSamplePos);
			terrainDistance = terrainSDF(planet, sampleHeight, worldSamplePos.xz, /*out*/ normalMap);
			if(abs(terrainDistance) < RaymarchingSteps.z * currentT || terrainDistance < -50)
			{
				// Sufficient distance to claim as "hit"
				toDistance = currentT;
				return true;
			}

			currentT += QualitySettings_optimism * terrainDistance;
		}
		else
		{
			return false;
		}
	}
	if(terrainDistance < 1)
	{
		toDistance = currentT;
		return true;
	}
	return false;
}

// Reduced version (for shadows and light shafts), that uses different uniform parameters
bool raymarchTerrainL(Planet planet, Ray ray, float fromDistance, float toDistance)
{
	float currentT = fromDistance;
	toDistance = min(toDistance, LightSettings_farPlane);

	for(int i = 0; i < floatBitsToInt(LightSettings_shadowSteps);i++)
	{
		vec2 normalMap;
		vec3 sphNormal;
		vec3 worldSamplePos;

		float sampleHeight = getSampleParameters(planet, ray, currentT, /*out*/sphNormal, /*out*/worldSamplePos);
		float terrainDistance = terrainSDF(planet, sampleHeight, worldSamplePos.xz, /*out*/ normalMap);
		if(terrainDistance < LightSettings_precision * currentT)
		{
			// Sufficient distance to claim as "hit"
			return true;
		}
		if(terrainDistance > toDistance)
			return false;

		currentT += QualitySettings_optimism * terrainDistance;
	}
	return false;
}

// "Dynamic" cascaded version
bool raymarchTerrainD(Planet planet, Ray ray, float fromDistance, float toDistance)
{
	float currentT = fromDistance;
	toDistance = min(toDistance, LightSettings_farPlane);

	vec3 worldSamplePos = Camera_position;
	for(int i = 0; i < floatBitsToInt(LightSettings_shadowSteps);i++)
	{
		vec2 normalMap;
		vec3 sphNormal;
		float sampleHeight;
		if(distance(worldSamplePos, Camera_position) > LightSettings_shadowCascade)
		{
			sampleHeight = getSampleParameters(planet, ray, currentT, /*out*/sphNormal, /*out*/worldSamplePos);
		}
		else
		{
			sampleHeight = getSampleParametersH(planet, ray, currentT, /*out*/sphNormal, /*out*/worldSamplePos);
		}
		
		float terrainDistance = terrainSDF(planet, sampleHeight, worldSamplePos.xz, /*out*/ normalMap);
		if(terrainDistance < LightSettings_precision * currentT)
		{
			// Sufficient distance to claim as "hit"
			return true;
		}
		if(terrainDistance > toDistance)
			return false;

		currentT += QualitySettings_optimism * terrainDistance;
	}
	return false;
}
#endif