//?#version 440
#include "Random.glsl"
#include "Buffers.glsl"
#include "Intersections.glsl"
#include "Lighting.glsl"

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
	float bigLod = lod-0.5;
	const float bigUvFrac = 10;
	vec2 biggerUV = triUVx/bigUvFrac;
	vec4 texX = textureLod(sampl, triUVx, lod);
	texX *= textureLod(sampl, biggerUV, bigLod);

	vec4 texY = textureLod(sampl, triUVy, lod);
	biggerUV = triUVy/bigUvFrac;
	texY *= textureLod(sampl, biggerUV, bigLod);

	vec4 texZ = textureLod(sampl, triUVz, lod);
	biggerUV = triUVz/bigUvFrac;
	texZ *= textureLod(sampl, biggerUV, bigLod);

	vec3 weights = abs(normal);
	weights /= (weights.x + weights.y + weights.z);

	vec4 color = texX * weights.x + texY * weights.y + texZ * weights.z;
	return color.xyz;
}

vec3 terrainColor(Planet planet, float T, vec3 pos, vec3 normal, float elev)
{
	// Triplanar texture mapping in world space
	float lod = pow(T, RaymarchingSteps.w) / RaymarchingSteps.y;
	float gradHeight = PlanetMaterial.w;
	if(elev < PlanetMaterial.x)
	{
		//return vec3(0);
		return triplanarSample(texSampler1, pos, normal, lod);
	}
	else if(elev < PlanetMaterial.x+gradHeight)
	{
		//return vec3(0,0,1);
		return mix(triplanarSample(texSampler1, pos, normal, lod), triplanarSample(texSampler2, pos, normal, lod),
					smoothstep(0,gradHeight,elev-PlanetMaterial.x));
	}
	else if(elev < PlanetMaterial.y)
	{
		//return vec3(0,1,0);
		return triplanarSample(texSampler2, pos, normal, lod);
	}
	else if(elev < PlanetMaterial.y+gradHeight)
	{
		//return vec3(0,1,1);
		return mix(triplanarSample(texSampler2, pos, normal, lod), triplanarSample(texSampler3, pos, normal, lod),
					smoothstep(0,gradHeight,elev-PlanetMaterial.y));
	}
	else
	{
		//return vec3(1,0,0);
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
	return mod(toUV(planetNormal)*500,1);
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

vec3 terrainShader(Planet p, float T, vec3 worldSamplePos, vec2 planetNormalMap, vec3 sphNormal, float sampleHeight)
{
	vec3 worldNormal = terrainNormal(planetNormalMap, sphNormal);
	if(DEBUG_NORMALS)
	{
		return worldNormal * 0.5 + 0.5;
	}
	return terrainColor(p, T, worldSamplePos, worldNormal, sampleHeight)
			* lightPoint(worldSamplePos, worldNormal);
}

bool raymarchTerrain(Planet planet, Ray ray, float fromDistance, inout float toDistance, out vec2 normalMap, out vec3 sphNormal, out vec3 worldSamplePos, out float sampleHeight)
{
	float t0, t1;
	if(!(raySphereIntersection(planet.center, planet.mountainsRadius, ray, t0, t1)&&t1>0))
		return false;

	fromDistance = max(fromDistance, t0);

	float currentT = fromDistance;
	float terrainDistance;

	for(int i = 0; i < floatBitsToInt(RaymarchingSteps.x);i++)
	{
		if(currentT <= QualitySettings_farPlane)
		{
			sampleHeight = getSampleParameters(planet, ray, currentT, /*out*/sphNormal, /*out*/worldSamplePos);
			terrainDistance = terrainSDF(planet, sampleHeight, sphNormal, /*out*/ normalMap);
			if(abs(terrainDistance) < RaymarchingSteps.z * currentT || terrainDistance < -100)
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

// Reduced version, that uses different uniform parameters
bool raymarchTerrainL(Planet planet, Ray ray, float fromDistance, float toDistance)
{
	float currentT = fromDistance;
	toDistance = min(toDistance, LightSettings_farPlane);

	for(int i = 0; i < floatBitsToInt(PlanetMaterial.z);i++)
	{
		vec2 normalMap;
		vec3 sphNormal;
		vec3 worldSamplePos;

		float sampleHeight = getSampleParameters(planet, ray, currentT, /*out*/sphNormal, /*out*/worldSamplePos);
		float terrainDistance = terrainSDF(planet, sampleHeight, sphNormal, /*out*/ normalMap);
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