/**
 * @author Ondøej Sabela
 * @brief Realistic Atmosphere - Thesis implementation.
 * @date 2021-2022
 * Copyright 2022 Ondøej Sabela. All rights reserved.
 * Uses ray tracing, path tracing and ray marching to create visually plausible outdoor scenes with atmosphere, terrain, clouds and analytical objects.
 */

//?#version 450
#include "Buffers.glsl"
#include "Intersections.glsl"
#include "Hit.glsl"
#include "Terrain.glsl"
#include "Lighting.glsl"
#include "Clouds.glsl"

int M_perAtmospherePixel = floatBitsToInt(Multisampling_perAtmospherePixel);

// Test for light shafts
float raymarchOcclusion(Planet planet, Ray ray, float fromT, float toT, bool viewTerrainHit, bool shadowed, DirectionalLight l)
{
	float t = fromT;
	float dx = (toT - fromT) / float(M_perAtmospherePixel);
	dx = max(dx, QualitySettings_minStepSize);// Otherwise too close object would evaluate too much steps
	float shadowLength = 0;
	float nu = dot(ray.direction, l.direction);
	for(int i = 0; i < M_perAtmospherePixel; i++,t+=dx)
	{
		vec3 worldPos = ray.origin + ray.direction * t;
		vec3 planetRelativePos = worldPos - planet.center;
		float r = length(planetRelativePos);
		vec3 sphNormal = planetRelativePos /r;
		float mu = dot(sphNormal, ray.direction);
		float mu_s = dot(sphNormal, l.direction);
		bool noSureIfEclipse = true;
		float lightFromT = 0, lightToT, _;

		if(HQSettings_lightShafts)
		{
			if(viewTerrainHit && LightSettings_rayAlsoShadowedThres != 0)
			{
				if(mu_s < LightSettings_viewThres || (shadowed && toT - t < LightSettings_rayAlsoShadowedThres))
				{
					shadowLength += dx;
					continue;//No light when sun is under the horizon
				}
				// When view ray and sun are on the opposite side, there nearly "should not be" any rays
				float diff = nu - LightSettings_noRayThres;
				// But it whould sometimes create a visible seam, so we create a gradient here
				if(diff < 0)
				{
					float gradientFactor = clamp(abs(diff) / LightSettings_gradient, 0, 1);
					if(gradientFactor > 0.999)
					{
						noSureIfEclipse = false;
					}
					else
					{
						shadowLength -= dx * gradientFactor;
					}
				}
				else
				{
					// We can start terrain raymarching at the distance of the terrain from the camera
					lightFromT = (toT - t) * LightSettings_terrainOptimMult;
				}
			}
			if(mu_s > LightSettings_noRayThres || t > QualitySettings_farPlane)
			{
				noSureIfEclipse = false;
			}
		
			// Intersect light ray with outer shell of the atmosphere
			Ray shadowRay = Ray(worldPos, l.direction);
			raySphereIntersection(planet.center, planet.atmosphereRadius,
								shadowRay, _, lightToT);
							
			//Firstly check for opaque object hits
			Hit hit = findObjectHit(shadowRay, false);
			lightToT = min(hit.t, lightToT);
			// Secondly check if sun is in shadow of the planet
			vec3 viewOnPlanetPlane = ray.direction - mu * sphNormal;
			vec3 sunOnPlanetPlane = l.direction - mu_s * sphNormal;

			if(noSureIfEclipse && r < planet.mountainsRadius)
			{
				//If we hit the mountains			
				if(raymarchTerrainL(planet, shadowRay, lightFromT, lightToT))
				{
					if(dot(sunOnPlanetPlane, viewOnPlanetPlane) > 0 && viewTerrainHit && t > toT * LightSettings_cutoffDist)
					{
						// In all the later samples, the sun will be also occluded
						return shadowLength + toT - t;//The later samples would all be occluded by the terrain
					}
					shadowLength += dx;
					continue;//Skip to next sample. This effectively creates light rays
				}
			}
			if(hit.hitObjectIndex != -1)
			{
				shadowLength += dx;
				continue;//Light is occluded by a object
			}
		}
		else
		{
			Ray shadowRay = Ray(worldPos, l.direction);
			raySphereIntersection(planet.center, planet.atmosphereRadius,
								shadowRay, _, lightToT);
							
			//Check ONLY for opaque object hits
			Hit hit = findObjectHit(shadowRay, false);
			if(hit.hitObjectIndex != -1)
			{
				shadowLength += dx;
				continue;//Light is occluded by a object
			}
		}
	}
	return shadowLength;
}

void precomputedAtmosphere(Planet p, Ray ray, float toT, bool terrainWasHit, bool terrainShadowed, inout vec3 luminance, inout vec3 transmittance)
{
	vec3 atmoTransmittance;
	float lightIndex = 0;
	vec3 planetSpaceCam = ray.origin - p.center;
	for(uint l = p.firstLight; l <= p.lastLight; l++)
	{
		DirectionalLight light = directionalLights[l];
		float shadow = HQSettings_lightShafts ? raymarchOcclusion(p, ray, 0, toT, terrainWasHit, terrainShadowed, light) : 0.0;
		luminance += GetSkyRadianceToPoint(p, transmittanceTable, singleScatteringTable,
											planetSpaceCam, toT, ray.direction, shadow, 
											light.direction, lightIndex, /*out*/ atmoTransmittance)
					* SkyRadianceToLuminance.rgb * transmittance;
		lightIndex++;
	}
	transmittance *= atmoTransmittance;
}

float raymarchAtmosphere(Planet planet, Ray ray, float minDistance, float maxDistance, float cloudsDistance, bool terrainWasHit, bool shadowed, inout vec3 luminance, inout vec3 transmittance);
bool terrainColorAndHit(Planet p, Ray ray, float fromDistance, inout float toDistance, inout vec3 throughput, inout vec3 luminance, inout Hit terrainHit)
{
	// Out parameters
	vec2 normalMap;
	vec3 sphNormal;
	vec3 worldSamplePos;
	float sampleHeight;

	if(raymarchTerrain(p, ray, fromDistance, /* inout */ toDistance,
			/* the rest params are "out" */
					normalMap, sphNormal, worldSamplePos, sampleHeight ))
	{
		vec3 worldNormal = terrainNormal(normalMap, sphNormal);
		terrainHit = Hit(worldSamplePos, worldNormal, UINT_MAX, toDistance);

		vec3 planetAlbedo = terrainColor(p, toDistance, worldSamplePos, worldNormal, sampleHeight);
		vec3 cloudsTrans, cloudsLum;
		float cloudsDistance = cloudsForPlanet(p,ray,fromDistance,toDistance,Clouds_terrainSteps,cloudsTrans,cloudsLum);

		bool shadowedByTerrain;
		vec3 light = planetIlluminance(p, terrainHit, /*out*/ shadowedByTerrain);
		if(!DEBUG_ATMO_OFF)
		{
			//Compute atmosphere contribution between terrain and camera
			if(HQSettings_atmoCompute)
			{
				if(cloudsDistance > 0)
				{
					raymarchAtmosphere(p, ray, fromDistance, toDistance, cloudsDistance, true, shadowedByTerrain, /*inout*/ luminance, /*inout*/ throughput);
					luminance += cloudsLum * throughput;
					throughput *= cloudsTrans;
					if(throughput.r > 0.001 || throughput.g > 0.001 || throughput.b > 0.001)
					{
						raymarchAtmosphere(p, ray, cloudsDistance, toDistance, toDistance, true, shadowedByTerrain, /*inout*/ luminance, /*inout*/ throughput);
					}
				}
				else
				{
					raymarchAtmosphere(p, ray, fromDistance, toDistance, toDistance, true, shadowedByTerrain, /*inout*/ luminance, /*inout*/ throughput);
				}
			}
			else
			{
				ray.origin += ray.direction * fromDistance;
				cloudsDistance -= fromDistance;
				toDistance -= fromDistance;
				if(cloudsDistance > 0)
				{
					precomputedAtmosphere(p, ray, cloudsDistance, true, shadowedByTerrain, luminance, throughput);
					luminance += cloudsLum * throughput;
					throughput *= cloudsTrans;

					ray.origin += ray.direction * cloudsDistance;
					toDistance=- cloudsDistance;
				}
				if(throughput.r > 0.001 || throughput.g > 0.001 || throughput.b > 0.001)
				{
					precomputedAtmosphere(p, ray, toDistance, true, shadowedByTerrain, luminance, throughput);
				}
			}
		}
		else
		{
			luminance += cloudsLum * throughput;
			throughput *= cloudsTrans;
		}
		luminance += planetAlbedo * light * throughput;
		throughput *= planetAlbedo;
		return true;
	}
	return false;
}

/**
  * Executes a raymarching through the atmosphere, clouds and terrian or returns the precomputed values.
  * @returns false when only atmosphere or nothing was hit
  * @param tMax By tweaking the tMax parameter, you can limit the assumed ray length
  * This is useful when the ray was blocked by some objects
  */
bool planetsWithAtmospheres(Ray ray, float tMax/*some object distance*/, inout vec3 luminance, inout vec3 throughput, out Hit planetHit)
{
	planetHit.t = POSITIVE_INFINITY;
	for (int k = 0; k < planets.length(); ++k)
    {
        Planet p = planets[k];
		float fromDistance, toDistance;
		if(!raySphereIntersection(p.center, p.atmosphereRadius, ray, /*out*/ fromDistance, /*out*/ toDistance)
		|| (toDistance < 0 ))// this would mean that the atmosphere and the planet is behind us
		{
			return false;
		}
        float surfaceDistance, t1; 
		bool surfaceIntersection;
        if (surfaceIntersection = raySphereIntersection(p.center, p.surfaceRadius, ray, surfaceDistance, t1) && t1 > 0) 
        {
			planetHit.t = surfaceDistance;
			tMax = min(tMax, max(0, surfaceDistance));//Limit by planet surface or "some object" distance
        }

		//Limit the computation bounds according to the Hit
		toDistance = min(tMax, toDistance);
		//Limit the srating point to the screen
		fromDistance = max(fromDistance, 0);
		if(terrainColorAndHit(p, ray, fromDistance, toDistance, throughput, luminance, planetHit))
		{
			return true;
		}
		// Only atmosphere is in ray's path
		vec3 cloudTrans, cloudLum;
		float cloudsDistance = cloudsForPlanet(p,ray,fromDistance,toDistance,Clouds_iter,cloudTrans,cloudLum);
		if(!DEBUG_ATMO_OFF)
		{
			if(HQSettings_atmoCompute)
			{
				if(cloudsDistance > 0)
				{
					raymarchAtmosphere(p, ray, fromDistance, toDistance, cloudsDistance, false, false, /*inout*/ luminance, /*inout*/ throughput);
					luminance += cloudLum * throughput;
					throughput *= cloudTrans;
					if(throughput.r > 0.001 || throughput.g > 0.001 || throughput.b > 0.001)
					{
						raymarchAtmosphere(p, ray, cloudsDistance, toDistance, toDistance, false, false, /*inout*/ luminance, /*inout*/ throughput);
					}
				}
				else
				{
					raymarchAtmosphere(p, ray, fromDistance, toDistance, toDistance, false, false, /*inout*/ luminance, /*inout*/ throughput);
				}
			}
			else
			{
				ray.origin += ray.direction * fromDistance;
				cloudsDistance -= fromDistance;
				toDistance -= fromDistance;
				if(cloudsDistance > 0)
				{
					precomputedAtmosphere(p, ray, cloudsDistance, false, false, luminance, throughput);
					luminance += cloudLum * throughput;
					throughput *= cloudTrans;
					ray.origin += ray.direction * cloudsDistance;
					toDistance -= cloudsDistance;
				}
				precomputedAtmosphere(p, ray, toDistance, false, false, luminance, throughput);
			}
		}
		else
		{
			luminance += cloudLum * throughput;
			throughput *= cloudTrans;
		}
		
		vec3 worldHitPos = ray.origin + ray.direction * toDistance;
		planetHit.position = worldHitPos;
		planetHit.normalAtHit = normalize(worldHitPos - p.center);
		planetHit.hitObjectIndex = -1;
		return false;
	}
}

// Atmosphere raymarching process
float raymarchAtmosphere(Planet planet, Ray ray, float minDistance, float maxDistance, float cloudsDistance, bool terrainWasHit, bool shadowed, inout vec3 luminance, inout vec3 transmittance)
{
	float t0, t1;

	float pathFraction = (maxDistance - minDistance) / M_perAtmospherePixel;
	float segmentLength = max(pathFraction, QualitySettings_minStepSize);// Otherwise too close object would evaluate too much steps

	vec3 rayleighColor = vec3(0);
	vec3 mieColor = vec3(0);
	float mieExtinction = 1.11 * planet.mieCoefficient;
	float opticalDepthR = 0, opticalDepthM = 0, opticalDepthO = 0; 

	float currentDistance;
	float i = 0.0;//float iterator for use in float operations
	int iter = 0;
	int toCloudsIterations = max(int(ceil((cloudsDistance - minDistance) / segmentLength)),1);

	for(currentDistance = minDistance; iter < toCloudsIterations; currentDistance += segmentLength, iter++, i++)
	{
		// Always sample at the center of sample
		vec3 worldSamplePos;
		vec3 sphNormal;
		float sampleHeight = getSampleParameters(planet, ray, currentDistance + segmentLength * 0.5, /*out*/ sphNormal, /*out*/ worldSamplePos);

		//Compute HF = height factor
		float rayleighHF = exp(-sampleHeight/planet.rayleighScaleHeight) * segmentLength;
		float mieHF = exp(-sampleHeight/planet.mieScaleHeight) * segmentLength;
		opticalDepthO += ozoneHF(sampleHeight, planet, segmentLength);
		opticalDepthR += rayleighHF; 
        opticalDepthM += mieHF;

		for(uint l = planet.firstLight; l <= planet.lastLight; l++)
		{
			//
			//Compute optical depth for every light
			//
			DirectionalLight light = directionalLights[l];
			float lightToViewCos /*nu*/ = dot(light.direction, ray.direction);
			float sunToNormalCos /*mu_s*/ = dot(light.direction, sphNormal);

			float lightFromT = 0, dummy, lightToT; 
			// Intersect light ray with outer shell of the planet
			Ray shadowRay = Ray(worldSamplePos, light.direction);
			raySphereIntersection(planet.center, planet.atmosphereRadius,
								shadowRay, dummy, lightToT);

			//Firstly check for object hits
			Hit hit = findObjectHit(shadowRay, false);
			if(HQSettings_lightShafts)// Only raymarch terrain if the user wants to
			{
				lightToT = min(hit.t, lightToT);
				// Secondly check if sun is in shadow of the planet
				vec3 viewOnPlanetPlane = ray.direction - dot(ray.direction,sphNormal) * sphNormal;
				vec3 sunOnPlanetPlane = light.direction - sunToNormalCos * sphNormal;

				bool noSureIfEclipse = true;
				if(terrainWasHit && LightSettings_rayAlsoShadowedThres != 0)
				{
					if(sunToNormalCos < LightSettings_viewThres || (shadowed && maxDistance - currentDistance < LightSettings_rayAlsoShadowedThres))
					{
						continue;//No light when sun is under the horizon
					}
					// When view ray and sun are on the opposite side, there nearly "should not be" any rays
					float diff = lightToViewCos - LightSettings_noRayThres;
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
				if(sunToNormalCos > LightSettings_noRayThres // Terrain is never above the viewer, so we can skip testing for shadowing in a conic area above the camera
					|| currentDistance > QualitySettings_farPlane)
				{
					noSureIfEclipse = false;
				}

				if(noSureIfEclipse && distance(worldSamplePos, planet.center) < planet.mountainsRadius)
				{
					//If we hit the mountains			
					if(raymarchTerrainL(planet, shadowRay, lightFromT, lightToT))
					{
						if(dot(sunOnPlanetPlane,viewOnPlanetPlane) > 0 && terrainWasHit && currentDistance > maxDistance * LightSettings_cutoffDist)
						{
							// If the view ray has direction towards the center of the planet more than towards the sky
							// In all the later samples, the sun will be also occluded
							break;//The later samples would all be occluded by the terrain
						}
						continue;//Skip to next sample. This effectively creates light rays
					}
				}
			}
			if(hit.hitObjectIndex != -1)
			{
				continue;//Light is occluded by a object
			}

			// The lookup table is from alpha angle value from -0.5 to 1.0, so we must remap the X coord
			vec2 tableCoords = vec2((0.5 + sunToNormalCos)/1.5, sampleHeight/(planet.atmosphereThickness));
			vec4 lOpticalDepth = texture(opticalDepthTable, tableCoords);

			//Finalize the computation and commit to the result color
			vec3 depth = planet.rayleighCoefficients * (lOpticalDepth.x + opticalDepthR)
							+ mieExtinction * (lOpticalDepth.y + opticalDepthM)
							+ planet.absorptionCoefficients * (lOpticalDepth.z + opticalDepthO);
			vec3 attenuation = exp(-depth);
			rayleighColor += attenuation * rayleighHF * light.irradiance;
			mieColor += attenuation * mieHF * light.irradiance;
		}
	}

	for(uint l = planet.firstLight; l <= planet.lastLight; l++)
	{
		DirectionalLight light = directionalLights[l];
		float sunsetArtifactSolution = smoothstep(0.0, 0.01, dot(light.direction, normalize(ray.origin - planet.center)));

		float lightToViewCos /*nu*/ = dot(light.direction, ray.direction);

		float nu2 = lightToViewCos * lightToViewCos;
		vec3 planetRelativePos = ray.origin - planet.center;
		vec3 relPosNorm = normalize(planetRelativePos);
		float mu = dot(ray.direction, relPosNorm);
		float mu_l = dot(light.direction, relPosNorm);

		float rayleightPhase = 3.0 / (16.0 * pi) * (1 + nu2);
		//There should be extinction coefficients, but for Rayleigh, they are the same as scattering coeff.s and for Mie, it is 1.11 times the s.c.
		float assymetryFactor2 = planet.mieAsymmetryFactor * planet.mieAsymmetryFactor;
		float miePhase = 3.0 /
			(8.0 * pi) * ((1.0 - assymetryFactor2) * (1.0 + (nu2)))
			/ ((2.f + assymetryFactor2) * pow(1.0 + assymetryFactor2 - 2.0 * planet.mieAsymmetryFactor * lightToViewCos, 1.5f));
		// Add atmosphere to planet color /* or to nothing */
		luminance += (rayleighColor * planet.rayleighCoefficients * rayleightPhase
				+ mieColor * planet.mieCoefficient * miePhase * sunsetArtifactSolution
				) * transmittance * SkyRadianceToLuminance.rgb;
	}
	vec3 depth = planet.rayleighCoefficients * opticalDepthR
						+ mieExtinction * opticalDepthM
						+ planet.absorptionCoefficients * opticalDepthO;
	vec3 attenuation = exp(-depth);
	transmittance *= attenuation;
	return currentDistance;
}