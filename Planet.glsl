//?#version 450
#include "Buffers.glsl"
#include "Intersections.glsl"
#include "Atmosphere.glsl"
#include "Terrain.glsl"
#include "Lighting.glsl"
#define PI pi

float raymarchPlanet(Planet planet, Ray ray, float minDistance, float maxDistance, out vec3 color);

/**
  * Does a raymarching through the atmosphere and planet
  * @returns 0 when only atmosphere or nothing was hit, else a number higher than 0 (when the planet was hit)
  * @param tMax By tweaking the tMax parameter, you can limit the assumed ray length
  * This is useful when the ray was blocked by some objects
  */
float planetsWithAtmospheres(Ray ray, float tMax/*some object distance*/, out vec3 color)
{
	color = vec3(0);
	for (int k = 0; k < planets.length(); ++k)
    {
        Planet p = planets[k];
        float t0, t1, tMax = POSITIVE_INFINITY; 
		float fromDistance, toDistance;
		if(!raySphereIntersection(p.center, p.atmosphereRadius, ray, fromDistance, toDistance)
		|| (t1 < 0 ))// this would mean that the atmosphere and the planet is behind us
		{
			return 0;
		}
        if (raySphereIntersection(p.center, p.surfaceRadius, ray, t0, t1) && t1 > 0) 
        {
            tMax = min(tMax, max(0, t0));//Limit by planet surface or "some object" distance
        }
		//Limit the computation bounds according to the Hit
		toDistance = min(tMax, toDistance);

		return raymarchPlanet(p, ray, max(fromDistance,0), toDistance, /*out*/ color);
	}
}

float raymarchPlanet(Planet planet, Ray ray, float minDistance, float maxDistance, out vec3 color)
{
	color = vec3(0);
	float t0, t1;
	bool terrainCanBeHit = raySphereIntersection(planet.center, planet.mountainsRadius, ray, t0, t1) && t1 > 0;

	float segmentLength = (maxDistance - minDistance) / Multisampling_perAtmospherePixel;
	float currentDistance = minDistance;
	float previousDistance;

	vec3 rayleighColor = vec3(0);
	vec3 mieColor = vec3(0);
	float opticalDepthR = 0, opticalDepthM = 0; 

	vec3 sunVector = directionalLights[planet.sunDrectionalLightIndex].direction.xyz;
	float angleDot = dot(sunVector, ray.direction);

	float rayleightPhase = 3.0 / (16.0 * PI) * (1 + angleDot * angleDot);
	float assymetryFactor2 = planet.mieAsymmetryFactor * planet.mieAsymmetryFactor;
	float miePhase = 3.0 /
		(8.0 * PI) * ((1.0 - assymetryFactor2) * (1.0 + (angleDot * angleDot)))
		/ ((2.f + assymetryFactor2) * pow(1.0 + assymetryFactor2 - 2.0 * planet.mieAsymmetryFactor * angleDot, 1.5f));

	for(int i = 0; i < Multisampling_perAtmospherePixel; i++)
	{
		// Always sample at the center of sample
		vec3 worldSamplePos;
		vec3 sphNormal;
		previousDistance = currentDistance;
		float sampleHeight = getSampleParameters(planet, ray, currentDistance, /*out*/ sphNormal, /*out*/ worldSamplePos);

		// Check if there is terrain at this sample
		if(terrainCanBeHit)
		{
			vec2 planetNormalMap;
			float terrainDistance = terrainSDF(planet, sampleHeight, sphNormal, /*out*/ planetNormalMap);
			if(terrainDistance < 0)
			{	
				//We could now return the color of the planet, but instead we want to do a binary search for a more precise intersection
				vec3 betterIntersection = bisectTerrain(planet, ray, previousDistance, currentDistance,
										/*out*/ planetNormalMap, /*out*/ sphNormal);
				vec3 worldNormal = terrainNormal(planetNormalMap, sphNormal);
				if(DEBUG_NORMALS)
				{
					//color = worldNormal * 0.5 + 0.5;
					color = vec3(length(worldSamplePos - betterIntersection)/1000);
					return currentDistance;
				}
				color = terrainColor(planet, ray.origin, betterIntersection, worldNormal);
				break; // Skip further atmosphere raymarching
			}
		}


		//Compute optical depth; HF = height factor
		float rayleighHF = exp(-sampleHeight/planet.rayleighScaleHeight) * segmentLength;
		float mieHF = exp(-sampleHeight/planet.mieScaleHeight) * segmentLength;
		opticalDepthR += rayleighHF; 
        opticalDepthM += mieHF;

		//Compute light optical depth
        float t0Light, t1Light; 
		// Intersect light ray with outer shell of the planet
        raySphereIntersection(planet.center, planet.atmosphereRadius, Ray(worldSamplePos, sunVector), t0Light, t1Light); 
        float lSegmentLength = t1Light / Multisampling_perLightRay;
		float tCurrentLight = 0; 
        float lOpticalDepthR = 0, lOpticalDepthM = 0; 

		int l;
		for (l = 0; l < Multisampling_perLightRay; ++l) { 
            vec3 lSamplePos = worldSamplePos + (tCurrentLight + lSegmentLength * 0.5) * sunVector; 
			float lCenterDist = distance(lSamplePos, planet.center);
            float lSampleHeight = lCenterDist - planet.surfaceRadius; 
            if (lSampleHeight < 0) break;

            lOpticalDepthR += exp(-lSampleHeight / planet.rayleighScaleHeight) * lSegmentLength; 
            lOpticalDepthM += exp(-lSampleHeight / planet.mieScaleHeight) * lSegmentLength; 
            tCurrentLight += lSegmentLength; 
        }
		if(l == Multisampling_perLightRay)
		{
			//Finalize the computation and commit to the result color

			//There should be extinction coefficients, but for Rayleigh, they are the same as scattering coeff.s and for Mie, it is 1.11 times the s.c.
			float mieExtinction = 1.11 * planet.mieCoefficient;
			vec3 depth = planet.rayleighCoefficients * (lOpticalDepthR + opticalDepthR)
							+ mieExtinction * (lOpticalDepthM + opticalDepthM);
			vec3 attenuation = exp(-depth);

			rayleighColor += attenuation * rayleighHF;
			mieColor += attenuation * mieHF;
		}
		// Shift to next sample
		currentDistance += segmentLength;
	}
	// Add atmosphere to planet color /* or to nothing */
	color += (rayleighColor * planet.rayleighCoefficients * rayleightPhase
			+ mieColor * planet.mieCoefficient * miePhase
			) * planet.sunIntensity;
	return currentDistance;
}