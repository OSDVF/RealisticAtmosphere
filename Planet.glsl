//?#version 450
#include "Buffers.glsl"
#include "Intersections.glsl"
#include "Random.glsl"
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
vec3 triplanarSample(sampler2D sampl, Hit hit)
{
	vec2 triUVx = (hit.position.zy)/100;
	vec2 triUVy = (hit.position.xz)/100;
	vec2 triUVz = (hit.position.xy)/100;
	#ifdef COMPUTE
	vec4 texX = textureLod(sampl, triUVx, mip_map_level(hit.position.zy));
	vec4 texY = textureLod(sampl, triUVy, mip_map_level(hit.position.xz));
	vec4 texZ = textureLod(sampl, triUVz, mip_map_level(hit.position.xy));
	#else
	const float bigUvFrac = 10;
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

	vec3 weights = abs(hit.normalAtHit);
	weights /= (weights.x + weights.y + weights.z);

	vec4 color = texX * weights.x + texY * weights.y + texZ * weights.z;
	return color.xyz;
}

vec3 planetColor(Planet planet, vec3 camPos, Hit hit)
{
	// Triplanar texture mapping in world space
	float distFromSurface = distance(hit.position, planet.center) - planet.surfaceRadius;
	float elev = terrainElevation(hit.position);
	float gradHeight = 5000 * elev;
	float randomStrength = 10000;
	distFromSurface -= randomStrength * elev;
	if(distFromSurface < PlanetMaterial.x)
	{
		return triplanarSample(texSampler1, hit);
	}
	else if(distFromSurface < PlanetMaterial.x+gradHeight)
	{
		return mix(triplanarSample(texSampler1, hit),triplanarSample(texSampler2, hit),
					smoothstep(0,gradHeight,distFromSurface-PlanetMaterial.x));
	}
	else if(distFromSurface < PlanetMaterial.y)
	{
		return triplanarSample(texSampler2, hit);
	}
	else if(distFromSurface < PlanetMaterial.y+gradHeight)
	{
		return mix(triplanarSample(texSampler2, hit),triplanarSample(texSampler3, hit),
					smoothstep(0,gradHeight,distFromSurface-PlanetMaterial.y));
	}
	else
	{
		return triplanarSample(texSampler3, hit);
	}
}

// https://www.shadertoy.com/view/WsySzw
float pow3(float f) {
    return f * f * f;
}

vec3 biLerp(vec3 a, vec3 b, vec3 c, vec3 d, float s, float t)
{
  vec3 x = mix(a, b, t);
  vec3 y = mix(c, d, t);
  return mix(x, y, s);
}

bool raymarchTerrain(Planet planet, Ray ray, float minDistance, float maxDistance, out Hit hitRecord, out float t) {
	int backSteps = 0, forwardStepsBig = 0, forwardStepsSmall = 0;
	float t0, t1;
	raySphereIntersection(planet.center, planet.surfaceRadius, ray, t0, t1);
	float mountainHeight = planet.mountainsRadius -  planet.surfaceRadius;

	if(t0 < maxDistance && t0 > 0) {
		maxDistance = t0;
	}
	// Limit to the far plane
	maxDistance = min(maxDistance, QualitySettings_farPlane);

	float fixedSegmentLength = distance((ray.origin + ray.direction * maxDistance),
										(ray.origin + ray.direction * minDistance))/QualitySettings_steps;
	t = minDistance;
	ivec2 mapSize = textureSize(heightmapTexture,0);

	vec3 bump;
	bool wasHit = false;
	vec3 sphNormal;
	vec3 samplePos;
	int i;
    for (i = 0; i < QualitySettings_steps; i++) {
        samplePos = ray.origin + ray.direction * t;
		
		sphNormal = normalize(samplePos - planet.center);
		#if 1
		// Perform hermite bilinear interpolation of texture
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

		float surfaceHeight = planet.surfaceRadius + (bump.x * mountainHeight);
		vec3 surfacePoint = planet.center + surfaceHeight * sphNormal;
		float sampleHeight = distance(planet.center, samplePos);

		// Compute "distance" function
		float dist = sampleHeight - surfaceHeight;
		if(dist < 0)
		{
			wasHit = true;
			backSteps++;
			t += min(dist * RaymarchingSteps.z, RaymarchingSteps.w);
		}
		else 
		{
			if(dist < QualitySettings_precision)
			{
				wasHit = true;
				break;
			}
			else if(dist > maxDistance)// When we are too far away from the surface
			{
				return false;
			}

			int remainingSteps = int(QualitySettings_steps/2/*Fixed step will be 2*long as 'optimalisation' */) - i;
			float remainingDistance = maxDistance - t;

			float realisticStep;
			if(wasHit)
			{
				t += (dist*RaymarchingSteps.x)/remainingSteps;
				continue;
			}
			else
				realisticStep = remainingDistance/remainingSteps;
			float optimisticStep = dist * QualitySettings_optimism;
			float slopeFactor = sqrt(1 - dot(bump.gb, bump.gb));
			vec3 planetPointApprox = samplePos;
			planetPointApprox -= sphNormal*dist;
			float biggerStep = max(realisticStep,optimisticStep);
			float smallerStep = min(realisticStep,optimisticStep);

			float mixFactor = max(slopeFactor, 1-(distance(ray.origin,planetPointApprox))/RaymarchingSteps.y);
			if(mixFactor > 0.5)
			{
				forwardStepsSmall++;
			}
			else
			{
				forwardStepsBig++;
			}
			float st = mix(biggerStep,smallerStep,mixFactor);
			if(t + st > maxDistance)
			{
				t = maxDistance;
			}
			else
			{
				t += st;
			}
		}
		if(t > maxDistance)
		{
			return false;
		}
    }
	if(wasHit)
	{
		vec3 worldNormal;
		if(DEBUG_RM)
		{
			worldNormal = vec3(forwardStepsBig,forwardStepsSmall,backSteps)/QualitySettings_steps;
		}
		else
		{
			// Compute normal in world space
			vec2 map = (bump.gb) * 2 - 1;
			vec3 t = sphereTangent(sphNormal);
			vec3 bitangent = sphNormal * t;
			float normalZ = sqrt(1-dot(map.xy, map.xy));
			worldNormal = normalize(map.x * t + map.y * bitangent + normalZ * sphNormal);
		}
		hitRecord = Hit(samplePos, worldNormal, -1);
		return true;
	}
	return false;
}
bool intersectsPlanet(Planet planet, Ray ray)
{
	float t0,t1;
	if(raySphereIntersection(planet.center, planet.mountainsRadius, ray, t0, t1) && t1 > 0)
    {
		// Compute intersection point with planet mountains by raymarching
		Hit outHit;
		if(raymarchTerrain(planet, ray, max(t0,0), t1, outHit , t1))
		{
			return true;
		}
    }
	return false;
}

vec3 raytracePlanet(Planet planet, Ray ray, out float tMax, out Hit outHit)
{
	tMax = POSITIVE_INFINITY;
	float t0,t1;
	if(raySphereIntersection(planet.center, planet.mountainsRadius, ray, t0, t1) && t1 > 0)
    {
		// Compute intersection point with planet mountains by raymarching
		float t2;
		if(raymarchTerrain(planet, ray, max(t0,0), t1, outHit, t2))
		{
			// When the ray intersects the planet surface, we need to limit the atmosphere
			// computation to the intersection point

			tMax = max(t2, 0);
			if(DEBUG_NORMALS)
			{
				return outHit.normalAtHit/2+0.5;
			}
			else if(DEBUG_RM)
			{
				return outHit.normalAtHit;
			}
			//return vec3(texture(heightmapTexture,toUV(normalize(outHit.position))).x);
			return planetColor(planet, ray.origin, outHit);
		}
    }
	return vec3(0);
}