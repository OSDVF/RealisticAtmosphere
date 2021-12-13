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

vec3 planetColor(vec3 camPos, Hit hit)
{
	float distanceFromCamera = distance(camPos, hit.position);
	//float lodLevel = distanceFromCamera / RaymarchingCascades.x;
	//float lodLevel = mip_map_level(hit.position.zy);
	// Triplanar texture mapping in world space
	vec2 triUVx = (hit.position.zy)/10;
	vec2 triUVy = (hit.position.xz)/10;
	vec2 triUVz = (hit.position.xy)/10;
	triUVx -= floor(triUVx);
	triUVy -= floor(triUVy);
	triUVz -= floor(triUVz);
	
	#ifdef COMPUTE
	vec4 texX = textureLod(texSampler1, triUVx, mip_map_level(hit.position.zy));
	vec4 texY = textureLod(texSampler2, triUVy, mip_map_level(hit.position.xz));
	vec4 texZ = textureLod(texSampler3, triUVz, mip_map_level(hit.position.xy));
	#else
	vec4 texX = textureGrad(texSampler1, triUVx, dFdx(triUVx),dFdy(triUVx));
	vec4 texY = textureGrad(texSampler2, triUVy, dFdx(triUVy),dFdy(triUVy));
	vec4 texZ = textureGrad(texSampler3, triUVz, dFdx(triUVz),dFdy(triUVz));
	#endif

	vec3 weights = abs(hit.normalAtHit);
	weights /= (weights.x + weights.y + weights.z);

	vec4 color = texX * weights.x + texY * weights.y + texZ * weights.z;
	return color.xyz;
}

// https://www.shadertoy.com/view/WsySzw
float pow3(float f) {
    return f * f * f;
}
/*vec4 terrainElevation(vec3 p, Planet planet) {
    vec3 surfaceLocation = normalize(p - planet.center);
	float mountainHeight = planet.mountainsRadius -  planet.surfaceRadius;
    vec4 elevation = noised(surfaceLocation * 400);
	elevation.x *= mountainHeight;
    return elevation;
}

// Signed distance function describing the terrain.
float terrainSdf(vec3 p, Planet planet) {
    float elevation = terrainElevation(p, planet).x;
    return distance(planet.center, p) - elevation - planet.surfaceRadius;
}

// normal function from Graphics Codex
vec3 terrianNormal(vec3 p, Planet planet) {
    p = normalize(p);
    const float e = 1e-2;
    const vec3 u = vec3(e, 0, 0);
    const vec3 v = vec3(0, e, 0);
    const vec3 w = vec3(e, 0, e);
    
    return normalize(vec3(
        terrainSdf(p + u, planet) - terrainSdf(p - u, planet),
        terrainSdf(p + v, planet) - terrainSdf(p - v, planet),
        terrainSdf(p + w, planet) - terrainSdf(p - w, planet)));
}*/

vec3 biLerp(vec3 a, vec3 b, vec3 c, vec3 d, float s, float t)
{
  vec3 x = mix(a, b, t);
  vec3 y = mix(c, d, t);
  return mix(x, y, s);
}

bool raymarchTerrain(Planet planet, Ray ray, float minDistance, float maxDistance, out Hit hitRecord, out float t) {
	
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
		#if 0
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
			t += min(dist * RaymarchingSteps.z, RaymarchingSteps.w);
		}
		else 
		{
			if(dist < QualitySettings_precision || surfaceHeight >= sampleHeight)
			{
				wasHit = true;
				break;
			}
			else if(dist > maxDistance)// When we are too far away from the surface
			{
				return false;
			}

			int remainingSteps = int(QualitySettings_steps) - i;
			float remainingDistance = maxDistance - t;

			//t += max(minSegmentLength, dist * QualitySettings_optimism);
			float realisticStep;
			/*if(t < RaymarchingCascades.x)
				realisticStep = RaymarchingSteps.x;
			else if(t < RaymarchingCascades.y)
				realisticStep = RaymarchingSteps.y;
			else*/
				realisticStep = remainingDistance/remainingSteps;
			float optimisticStep = dist;
			if(optimisticStep > realisticStep)
			{
				if(t + optimisticStep > maxDistance)
				{
					t = maxDistance;
				}
				else
				{
					t += optimisticStep;
				}
			}
			else
			{
				t += realisticStep;
			}
		}
		if(t > maxDistance)
		{
			return false;
		}
    }
	if(wasHit)
	{
		// Compute normal in world space
		vec2 map = bump.gb * 2 - 1;
		vec3 t = sphereTangent(sphNormal);
		vec3 bitangent = sphNormal * t;
		float normalZ = sqrt(1 - dot(map.xy, map.xy));
		vec3 worldNormal = normalize(map.x * t + map.y * bitangent + normalZ * sphNormal);
		hitRecord = Hit(samplePos, worldNormal, i);
		return true;
	}
	return false;
}

vec3 raytracePlanet(Planet planet, Ray ray, out float tMax)
{
	tMax = POSITIVE_INFINITY;
	float t0,t1;
	if(raySphereIntersection(planet.center, planet.mountainsRadius, ray, t0, t1) && t1 > 0)
    {
		// Compute intersection point with planet mountains by raymarching
		Hit outHit;
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
				return vec3(outHit.hitObjectIndex)/QualitySettings_steps;
			}
			//return vec3(texture(heightmapTexture,toUV(normalize(outHit.position))).x);
			return planetColor(ray.origin, outHit);
		}
    }
	return vec3(0);
}