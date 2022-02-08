//?#version 450
#include "Buffers.glsl"
#include "Intersections.glsl"
#include "Hit.glsl"
#include "Terrain.glsl"
#include "Lighting.glsl"

uniform sampler2D opticalDepthTable;
#define PI pi

float raymarchAtmosphere(Planet planet, Ray ray, float minDistance, float maxDistance, inout vec3 radiance, inout vec3 transmittance, bool terrainWasHit);

/**
  * Does a raymarching through the atmosphere and planet
  * @returns 0 when only atmosphere or nothing was hit, else a number higher than 0 (when the planet was hit)
  * @param tMax By tweaking the tMax parameter, you can limit the assumed ray length
  * This is useful when the ray was blocked by some objects
  */
bool planetsWithAtmospheres(Ray ray, float tMax/*some object distance*/, out vec3 radiance, inout vec3 transmittance, out Hit planetHit)
{
	radiance = vec3(0);
	for (int k = 0; k < planets.length(); ++k)
    {
        Planet p = planets[k];
		float fromDistance, toDistance;
		if(!raySphereIntersection(p.center, p.atmosphereRadius, ray, /*out*/ fromDistance, /*out*/ toDistance)
		|| (toDistance < 0 ))// this would mean that the atmosphere and the planet is behind us
		{
			return false;
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
			vec3 worldNormal = terrainNormal(normalMap, sphNormal);
			vec3 planetAlbedo;
			if(DEBUG_NORMALS)
			{
				planetAlbedo = worldNormal * 0.5 + 0.5;
			}
			else
			{
				planetAlbedo = terrainColor(p, toDistance, worldSamplePos, worldNormal, sampleHeight);
			}
			if(!DEBUG_ATMO_OFF) raymarchAtmosphere(p, ray, fromDistance, toDistance, /*inout*/ radiance, /*inout*/ transmittance, true);
			radiance += planetAlbedo * lightPoint(worldSamplePos, worldNormal) * transmittance;
			transmittance *= planetAlbedo;
			planetHit = Hit(worldSamplePos, worldNormal, -1, toDistance);
			return true;
		}
		else if(!DEBUG_ATMO_OFF)
		{
			raymarchAtmosphere(p, ray, fromDistance, toDistance, /*inout*/ radiance,/*inout*/ transmittance, false);
		}
		return false;
	}
}

float raymarchAtmosphere(Planet planet, Ray ray, float minDistance, float maxDistance, inout vec3 radiance, inout vec3 transmittance, bool terrainWasHit)
{
	float t0, t1;

	float segmentLength = (maxDistance - minDistance) / floatBitsToInt(Multisampling_perAtmospherePixel);
	segmentLength = max(segmentLength, QualitySettings_minStepSize);// Otherwise too close object would evaluate too much steps

	vec3 rayleighColor = vec3(0);
	vec3 mieColor = vec3(0);
	vec3 lastAttenuation;
	float opticalDepthR = 0, opticalDepthM = 0; 

	vec3 sunVector = directionalLights[planet.sunDrectionalLightIndex].direction.xyz;
	float sunToViewCos = dot(sunVector, ray.direction);

	float rayleightPhase = 3.0 / (16.0 * PI) * (1 + sunToViewCos * sunToViewCos);
	//There should be extinction coefficients, but for Rayleigh, they are the same as scattering coeff.s and for Mie, it is 1.11 times the s.c.
	float mieExtinction = 1.11 * planet.mieCoefficient;
	float assymetryFactor2 = planet.mieAsymmetryFactor * planet.mieAsymmetryFactor;
	float miePhase = 3.0 /
		(8.0 * PI) * ((1.0 - assymetryFactor2) * (1.0 + (sunToViewCos * sunToViewCos)))
		/ ((2.f + assymetryFactor2) * pow(1.0 + assymetryFactor2 - 2.0 * planet.mieAsymmetryFactor * sunToViewCos, 1.5f));

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

        float lightFromT = 0, dummy, lightToT; 
		// Intersect light ray with outer shell of the planet
		Ray shadowRay = Ray(worldSamplePos, sunVector);
        raySphereIntersection(planet.center, planet.atmosphereRadius,
							shadowRay, dummy, lightToT);

		//Firstly check for object hits
		Hit hit = findObjectHit(shadowRay);
		lightToT = min(hit.t, lightToT);
		// Secondly check if sun is in shadow of the planet
		float sunToNormalCos = dot(sunVector, sphNormal);
		vec3 viewOnPlanetPlane = ray.direction - dot(ray.direction,sphNormal) * sphNormal;
		vec3 sunOnPlanetPlane = sunVector - sunToNormalCos * sphNormal;

		bool noSureIfEclipse = true;
		if(terrainWasHit)
		{
			if(sunToNormalCos < LightSettings_viewThres)
			{
				continue;//No light when sun is under the horizon
			}
			// When view ray and sun are on the opposite side, there nearly "should not be" any rays
			float diff = sunToViewCos - LightSettings_noRayThres;
			// But it whould sometimes create a visible seam, so we create a gradient here
			if(diff < 0 && mod(i, LightSettings_gradient) < 1)
			{
				noSureIfEclipse = false;
			}
			else
			{
				// We can start terrain raymarching at the distance of the terrain from the camera
				lightFromT = (maxDistance - currentDistance) * LightSettings_terrainOptimMult;
			}
		}
		if(sunToNormalCos > LightSettings_noRayThres || currentDistance > QualitySettings_farPlane)
		{
			if(DEBUG_RM)
				radiance = vec3(0,1,0);
			noSureIfEclipse = false;
		}

		if(noSureIfEclipse && distance(worldSamplePos, planet.center) < planet.mountainsRadius)
		{
			//If we hit the mountains			
			if(raymarchTerrainL(planet, shadowRay, lightFromT, lightToT))
			{
				if(dot(sunOnPlanetPlane,viewOnPlanetPlane) > 0 && terrainWasHit && currentDistance > maxDistance * LightSettings_cutoffDist)
				{
					if(DEBUG_RM)
						radiance = vec3(1,0,0);
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

		vec3 depth = planet.rayleighCoefficients * (lOpticalDepth.x + opticalDepthR)
						+ mieExtinction * (lOpticalDepth.y + opticalDepthM);
		vec3 attenuation = exp(-depth);
		rayleighColor += attenuation * rayleighHF;
		mieColor += attenuation * mieHF;
	}
	// Add atmosphere to planet color /* or to nothing */
	radiance += (rayleighColor * planet.rayleighCoefficients * rayleightPhase
			+ mieColor * planet.mieCoefficient * miePhase
			) * planet.sunIntensity * transmittance * SunRadianceToLuminance.rgb;
	vec3 depth = planet.rayleighCoefficients * opticalDepthR
						+ mieExtinction * opticalDepthM;
	vec3 attenuation = exp(-depth);
	transmittance *= attenuation;
	return currentDistance;
}