//?#version 450
#include "Buffers.glsl"
#include "Intersections.glsl"
#include "Hit.glsl"
#include "Terrain.glsl"
#include "Lighting.glsl"

uniform sampler2D opticalDepthTable;
#define PI pi

float raymarchPlanet(Planet planet, Ray ray, float minDistance, float maxDistance, inout vec3 color, bool terrainWasHit);

/**
  * Does a raymarching through the atmosphere and planet
  * @returns 0 when only atmosphere or nothing was hit, else a number higher than 0 (when the planet was hit)
  * @param tMax By tweaking the tMax parameter, you can limit the assumed ray length
  * This is useful when the ray was blocked by some objects
  */
void planetsWithAtmospheres(Ray ray, float tMax/*some object distance*/, inout vec3 color)
{
	for (int k = 0; k < planets.length(); ++k)
    {
        Planet p = planets[k];
		float fromDistance, toDistance;
		if(!raySphereIntersection(p.center, p.atmosphereRadius, ray, /*out*/ fromDistance, /*out*/ toDistance)
		|| (toDistance < 0 ))// this would mean that the atmosphere and the planet is behind us
		{
			return;
		}
        float t0, t1; 
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
			raymarchPlanet(p, ray, fromDistance, toDistance, /*inout*/ color, true);
		}
		else if(!DEBUG_ATMO_OFF)
		{
			raymarchPlanet(p, ray, fromDistance, toDistance, /*inout*/ color, false);
		}
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

float min3 (vec3 v) {
  return min (min (v.x, v.y), v.z);
}

float raymarchPlanet(Planet planet, Ray ray, float minDistance, float maxDistance, inout vec3 color, bool terrainWasHit)
{
	float t0, t1;

	float segmentLength = (maxDistance - minDistance) / Multisampling_perAtmospherePixel;
	segmentLength = max(segmentLength, QualitySettings_minStepSize);// Otherwise too close object would evaluate too much steps

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

	float currentDistance;
	float i = 0;
	for(currentDistance = minDistance; currentDistance < maxDistance; currentDistance += segmentLength, i++)
	{
		// Always sample at the center of sample
		vec3 worldSamplePos;
		vec3 sphNormal;
		float sampleHeight = getSampleParameters(planet, ray, currentDistance, /*out*/ sphNormal, /*out*/ worldSamplePos);

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

		//Firstly check for object hits
		Hit hit = findObjectHit(shadowRay);
		lightToT = min(hit.t, lightToT);
		// Secondly check if sun is in shadow of the planet
		float sunToNormalCos = dot(sunVector, sphNormal);
		vec3 viewOnPlanetPlane = ray.direction - dot(ray.direction,sphNormal) * sphNormal;
		vec3 sunOnPlanetPlane = sunVector - sunToNormalCos * sphNormal;
		float sunToViewCos = dot(sunVector, ray.direction);

		bool noSureIfEclipse = true;
		if(terrainWasHit)
		{
			if(sunToNormalCos < LightSettings_viewThres)
			{
				continue;//Skip to next sample. This effectively creates light rays
			}
			float diff = sunToViewCos - LightSettings_noRayThres;
			if(diff < 0 && mod(i, LightSettings_fieldThres) < 1)
			{
				noSureIfEclipse = false;
			}
		}
		if(sunToNormalCos > LightSettings_noRayThres || currentDistance > QualitySettings_farPlane)
		{
			if(DEBUG_RM)
				color = vec3(0,1,0);
			noSureIfEclipse = false;
		}

		if(noSureIfEclipse && distance(worldSamplePos, planet.center) < planet.mountainsRadius)
		{
			//If we hit the mountains			
			if(raymarchTerrainL(planet, shadowRay, 0, lightToT))
			{
				if(dot(sunOnPlanetPlane,viewOnPlanetPlane) > 0 && terrainWasHit && currentDistance > maxDistance * LightSettings_cutoffDist)
				{
					if(DEBUG_RM)
						color = vec3(1,0,0);
					// In all the later samples, the sun will be also occluded
					break;//The later samples would all be occluded by the terrain
				}
				continue;//Skip to next sample. This effectively creates light rays
			}
		}
		if(hit.hitObjectIndex != -1 && hit.hitObjectIndex != 0/*sun*/)
		{
			continue;//Light is occluded by a object
		}

		// The lookup table is from alpha angle value from -0.5 to 1.0, so we must remap the X coord
		vec2 tableCoords = vec2((0.5 + sunToNormalCos)/1.5, sampleHeight/(planet.atmosphereRadius - planet.surfaceRadius));
		vec4 lOpticalDepth = texture2D(opticalDepthTable, tableCoords);
		//Finalize the computation and commit to the result color

		//There should be extinction coefficients, but for Rayleigh, they are the same as scattering coeff.s and for Mie, it is 1.11 times the s.c.
		float mieExtinction = 1.11 * planet.mieCoefficient;
		vec3 depth = planet.rayleighCoefficients * (lOpticalDepth.x + opticalDepthR)
						+ mieExtinction * (lOpticalDepth.y + opticalDepthM);
		vec3 attenuation = exp(-depth);

		rayleighColor += attenuation * rayleighHF;
		mieColor += attenuation * mieHF;
	}
	// Add atmosphere to planet color /* or to nothing */
	color += (rayleighColor * planet.rayleighCoefficients * rayleightPhase
			+ mieColor * planet.mieCoefficient * miePhase
			) * planet.sunIntensity;
	return currentDistance;
}