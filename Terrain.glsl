//?#version 440
#include "Random.glsl"
#include "Buffers.glsl"
#include "Intersections.glsl"

float getSampleParameters(Planet planet, Ray ray, float currentDistance, out vec3 sphNormal, out vec3 worldSamplePos);

#ifdef COMPUTE
uvec2 quadIndex = gl_LocalInvocationID.xy / 2;
shared vec3 hitPosition;
#endif
vec3 triplanarSample(sampler2D sampl, vec3 pos, vec3 normal, float lod)
{
	pos /= 50;
	vec2 triUVx = (pos.zy);
	vec2 triUVy = (pos.xz);
	vec2 triUVz = (pos.xy);
	const float bigUvFrac = 10;
	vec2 biggerUV = triUVx/bigUvFrac;
	vec4 texX = textureLod(sampl, triUVx, lod);
	texX *= textureLod(sampl, biggerUV, lod);
	vec4 texY = textureLod(sampl, triUVy, lod);
	biggerUV = triUVy/bigUvFrac;
	texY *= textureLod(sampl, biggerUV, lod);
	vec4 texZ = textureLod(sampl, triUVz, lod);
	biggerUV = triUVz/bigUvFrac;
	texZ *= textureLod(sampl, biggerUV, lod);
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
	float elev = 1000;//terrainElevation(pos);
	float gradHeight = 5000 * elev;
	float randomStrength = 10000;
	distFromSurface -= randomStrength * elev;
	float lod = pow(distance(camPos,pos), RaymarchingSteps.w) / QualitySettings_precision;
	if(DEBUG_RM)
	{
		if(lod>4)
		{
			return vec3(1,1,0);
		}
		else if(lod > 3)
		{
			return vec3(1,0,0);
		}
		else if(lod > 2)
		{
			return vec3(0,1,0);
		}
		else if(lod > 1)
		{
			return vec3(0,0,1);
		}
		return vec3(0,0,0);
	}
	if(distFromSurface < PlanetMaterial.x)
	{
		return triplanarSample(texSampler1, pos, normal, lod);
	}
	else if(distFromSurface < PlanetMaterial.x+gradHeight)
	{
		return mix(triplanarSample(texSampler1, pos, normal, lod), triplanarSample(texSampler2, pos, normal, lod),
					smoothstep(0,gradHeight,distFromSurface-PlanetMaterial.x));
	}
	else if(distFromSurface < PlanetMaterial.y)
	{
		return triplanarSample(texSampler2, pos, normal, lod);
	}
	else if(distFromSurface < PlanetMaterial.y+gradHeight)
	{
		return mix(triplanarSample(texSampler2, pos, normal, lod), triplanarSample(texSampler3, pos, normal, lod),
					smoothstep(0,gradHeight,distFromSurface-PlanetMaterial.y));
	}
	else
	{
		return triplanarSample(texSampler3, pos, normal, lod);
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

vec2 planetUV(vec3 planetNormal)
{
	return mod(toUV(planetNormal)*100,1);
}

float terrainSDF(Planet planet, float sampleHeight /*above sea level*/, vec3 sphNormal, out vec2 outNormalMap)
{
	vec3 bump;
	#if 0
	// Perform hermite bilinear interpolation of texture
	ivec2 mapSize = textureSize(heightmapTexture,0);
	vec2 uvScaled = planetUV(sphNormal) * mapSize;
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
	bump = texture(heightmapTexture,planetUV(sphNormal)).xyz;
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
vec3 bisectTerrain(Planet planet, Ray ray, float fromT, /*inout*/ float toT, out vec2 outNormalMap, out vec3 sphNormal)
{
	vec3 worldSamplePos;
	float lastValue = 0;
	float currentT = (toT - fromT)/2;
	for(int i = 0; i < QualitySettings_steps; i++)
	{
		float sampleHeight = getSampleParameters(planet, ray, currentT, sphNormal, worldSamplePos);

		float terrainDistance = terrainSDF(planet, sampleHeight, sphNormal, outNormalMap);
		if(abs(terrainDistance - lastValue) < RaymarchingSteps.w)
		{
			return worldSamplePos;
		}
		if(abs(terrainDistance)<QualitySettings_precision)
				return worldSamplePos;
		/*if(terrainDistance < 0)
		{
			toT = currentT;
		}
		else
		{
			fromT = currentT;
		}*/
		currentT += terrainDistance;
		lastValue = terrainDistance;
	}
	return worldSamplePos;
}

bool raymarchTerrain(Planet planet, Ray ray, float fromDistance, inout float toDistance, out vec3 color)
{
	// Check if there is terrain at this sample
	float t0, t1;
	if(!(raySphereIntersection(planet.center, planet.mountainsRadius, ray, t0, t1)&&t1>0))
		return false;

	fromDistance = max(fromDistance, t0);

	float currentT = fromDistance;
	vec3 sphNormal, worldSamplePos;
	vec2 planetNormalMap;

	for(int i = 0; i < floatBitsToInt(RaymarchingSteps.x);i++)
	{
		if(currentT <= QualitySettings_farPlane)
		{
			float sampleHeight = getSampleParameters(planet, ray, currentT, sphNormal, worldSamplePos);
			float terrainDistance = terrainSDF(planet, sampleHeight, /*out*/ sphNormal, /*out*/ planetNormalMap);
			if(abs(terrainDistance) < RaymarchingSteps.y * currentT)
			{
				// Sufficient distance to claim as "hit"
				toDistance = currentT;
				//worldSamplePos = bisectTerrain(planet, ray, previousDistance, subStepDistance,
				//								/*out*/ planetNormalMap, /*out*/ sphNormal);
						//terrainColor(planet, ray.origin, worldSamplePos, worldNormal
				vec3 worldNormal = terrainNormal(planetNormalMap, sphNormal);
				if(DEBUG_NORMALS)
				{
					color = worldNormal * 0.5 + 0.5;
					//color = vec3(length(worldSamplePos - betterIntersection)/1000);
					//color = vec3(previousDistance,subStepDistance,terrainDistance);
					return true;
				}
				color = terrainColor(planet, ray.origin, worldSamplePos, worldNormal);
				return true;
			}

			currentT += QualitySettings_optimism * terrainDistance;
		}
		else
		{
			return false;
		}
	}
	return false;
}