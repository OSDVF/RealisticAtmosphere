//?#version 430
#include "Buffers.glsl"
#include "Intersections.glsl"
#include "Random.glsl"

vec3 planetColor(Hit hit)
{
	// Triplanar texture mapping in world space
	vec2 triUVx = mod((hit.position.zy)/1000,1);
	vec2 triUVy = mod((hit.position.xz)/1000,1);
	vec2 triUVz = mod((hit.position.xy)/1000,1);
	
	vec4 texX = texture(texSampler1, triUVx);
	vec4 texY = texture(texSampler1, triUVy);
	vec4 texZ = texture(texSampler1, triUVz);

	vec3 weights = abs(hit.normalAtHit);
	weights*=weights;
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

bool raymarchTerrain(Planet planet, Ray ray, float minDistance, float maxDistance, out Hit hitRecord, out float t) {
	
	float t0, t1;
	raySphereIntersection(planet.center, planet.surfaceRadius, ray, t0, t1);
	float mountainHeight = planet.mountainsRadius -  planet.surfaceRadius;

	if(t0 < maxDistance && t0 > 0) {
		maxDistance = t0;
	}
	maxDistance = min(maxDistance, QualitySettings_farPlane);

	float segmentLength = (maxDistance - minDistance)/QualitySettings_steps;

    for (t = minDistance; t < maxDistance; t += segmentLength) {
        vec3 samplePos = ray.origin + ray.direction * t;
		
		vec4 bump = texture(heightmapTexture,toUV(normalize(samplePos - planet.center)));

		float surfaceHeight = planet.surfaceRadius + (bump.x * mountainHeight);
		float sampleHeight = distance(planet.center, samplePos);

		if(sampleHeight - surfaceHeight < QualitySettings_precision)
		{
			hitRecord = Hit(samplePos, vec3(bump.gb,0), 0);
			return true;
		}
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
			};
			return vec3(texture(heightmapTexture,toUV(normalize(outHit.position))).x);
		}
    }
	return vec3(0);
}