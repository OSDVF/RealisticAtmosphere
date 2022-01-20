//?#version 450
#include "Buffers.glsl"
#include "Intersections.glsl"
#include "Terrain.glsl"
#include "Lighting.glsl"

uniform sampler2D opticalDepthTable;
#define PI pi

float raymarchPlanet(Planet planet, Ray ray, float minDistance, float maxDistance, inout vec3 color);

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
		//Limit the srating point to the screen
		fromDistance = max(fromDistance, 0);

		// Out parameters
		vec2 normalMap;
		vec3 worldSamplePos, sphNormal;
		float sampleHeight;

		if(raymarchTerrain(p, ray, fromDistance, /* inout */ toDistance,
			/* the rest params are "out" */
					normalMap, sphNormal, worldSamplePos, sampleHeight ))
		{
			color = terrainShader(p, toDistance, worldSamplePos, normalMap, sphNormal, sampleHeight);
		}
		if(DEBUG_ATMO_OFF)
		{
			return 0;
		}
		return raymarchPlanet(p, ray, fromDistance, toDistance, /*inout*/ color);
	}
}

float getSampleAtmParams(Planet planet, Ray ray, float currentDistance, out vec3 worldSamplePos)
{
	worldSamplePos = ray.origin + currentDistance * ray.direction;
	vec3 centerToSample = worldSamplePos - planet.center;
	float centerDist = length(centerToSample);
	// Height above the sea level
	return centerDist - planet.surfaceRadius;
}

float raymarchPlanet(Planet planet, Ray ray, float minDistance, float maxDistance, inout vec3 color)
{
	float t0, t1;

	float segmentLength = (maxDistance - minDistance) / Multisampling_perAtmospherePixel;
	float currentDistance = minDistance;
	float previousDistance = 0;

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
		float sampleHeight = getSampleAtmParams(planet, ray, currentDistance, /*out*/ worldSamplePos);

		//Compute optical depth; HF = height factor
		float rayleighHF = exp(-sampleHeight/planet.rayleighScaleHeight) * segmentLength;
		float mieHF = exp(-sampleHeight/planet.mieScaleHeight) * segmentLength;
		opticalDepthR += rayleighHF; 
        opticalDepthM += mieHF;

		//
		//Compute light optical depth
		//

        float lightFromT, lightToT; 
		// Intersect light ray with outer shell of the planet
		Ray shadowRay = Ray(worldSamplePos, sunVector);
        raySphereIntersection(planet.center, planet.atmosphereRadius,
							shadowRay, lightFromT, lightToT);
		
		if(distance(worldSamplePos, planet.center) < planet.mountainsRadius)
		{
			//If we hit the mountains			
			if(raymarchTerrainL(planet, shadowRay, 0, lightToT))
			{
				previousDistance = currentDistance;
				currentDistance += segmentLength;
				continue;//Skip to next sample. This effectively creates light rays
			}
		}
		//Firstly check if sun is in shadow of the planet
		float sunToViewAngleCos = dot(sunVector, normalize(worldSamplePos));
		/*
		if(sunToViewAngleCos < 0)
		{
			previousDistance = currentDistance;
			currentDistance += segmentLength;
			continue;//Skip to next sample
		}
		*/
		// The lookup table is from alpha angle value from -0.5 to 1.0, so we must remap the X coord
		vec2 tableCoords = vec2((0.5 + sunToViewAngleCos)/1.5, sampleHeight/(planet.atmosphereRadius - planet.surfaceRadius));
		vec4 lOpticalDepth = texture2D(opticalDepthTable, tableCoords);

        /*float lSegmentLength = lightToT / Multisampling_perLightRay;
		float tCurrentLight = 0; 
        float lOpticalDepthM = 0, lOpticalDepthR = 0; 

		int l;
		for (l = 0; l < Multisampling_perLightRay; ++l) { 
            vec3 lSamplePos = worldSamplePos + (tCurrentLight + lSegmentLength * 0.5) * sunVector; 
			float lCenterDist = distance(lSamplePos, planet.center);
            float lSampleHeight = lCenterDist - planet.surfaceRadius; 

            lOpticalDepthR += exp(-lSampleHeight / planet.rayleighScaleHeight) * lSegmentLength; 
            lOpticalDepthM += exp(-lSampleHeight / planet.mieScaleHeight) * lSegmentLength; 
            tCurrentLight += lSegmentLength; 
        }*/
		//if(l == Multisampling_perLightRay)
		{
			//Finalize the computation and commit to the result color

			//There should be extinction coefficients, but for Rayleigh, they are the same as scattering coeff.s and for Mie, it is 1.11 times the s.c.
			float mieExtinction = 1.11 * planet.mieCoefficient;
			vec3 depth = planet.rayleighCoefficients * (lOpticalDepth.x + opticalDepthR)
							+ mieExtinction * (lOpticalDepth.y + opticalDepthM);
			vec3 attenuation = exp(-depth);

			rayleighColor += attenuation * rayleighHF;
			mieColor += attenuation * mieHF;
		}
		// Shift to next sample
		previousDistance = currentDistance;
		currentDistance += segmentLength;
	}
	// Add atmosphere to planet color /* or to nothing */
	color += (rayleighColor * planet.rayleighCoefficients * rayleightPhase
			+ mieColor * planet.mieCoefficient * miePhase
			) * planet.sunIntensity;
	return currentDistance;
}