//?#version 450
#include "Buffers.glsl"
#include "Intersections.glsl"
#include "Hit.glsl"
#include "Terrain.glsl"
#include "Lighting.glsl"
#include "Clouds.glsl"

vec3 GetSkyRadiance(
    Planet planet,
    vec3 camera, vec3 view_ray, float shadow_length,
    vec3 lightVector, float lightIndex, out vec3 transmittance) {
  // Compute the distance to the top atmosphere boundary along the view ray,
  // assuming the viewer is in space (or NaN if the view ray does not intersect
  // the atmosphere).
  float r = length(camera);
  float rmu = dot(camera, view_ray);
  float distance_to_top_atmosphere_boundary = -rmu -
      sqrt(rmu * rmu - r * r + planet.atmosphereRadius * planet.atmosphereRadius);
  // If the viewer is in space and the view ray intersects the atmosphere, move
  // the viewer to the top atmosphere boundary (along the view ray):
  if (distance_to_top_atmosphere_boundary > 0.0) {
    camera = camera + view_ray * distance_to_top_atmosphere_boundary;
    r = planet.atmosphereRadius;
    rmu += distance_to_top_atmosphere_boundary;
  } else if (r > planet.atmosphereRadius) {
    // If the view ray does not intersect the atmosphere, simply return 0.
    transmittance = vec3(1.0);
    return vec3(0.0);
  }
  // Compute the r, mu, mu_l and nu parameters needed for the texture lookups.
  float mu = rmu / r;
  float mu_l = dot(camera, lightVector) / r;
  float nu = dot(view_ray, lightVector);
  bool ray_r_mu_intersects_ground = RayIntersectsGround(planet, r, mu);

  transmittance = ray_r_mu_intersects_ground ? vec3(0.0) :
      GetTransmittanceToTopAtmosphereBoundary(
          planet, transmittanceTable, r, mu);
  vec3 single_mie_scattering;
  vec3 scattering;
  if (shadow_length == 0.0) {
    scattering = GetCombinedScattering(
        planet, singleScatteringTable,
        r, mu, mu_l, nu, ray_r_mu_intersects_ground, lightIndex,
        single_mie_scattering);
  } else {
    // Case of light shafts (shadow_length is the total length noted l in our
    // paper): we omit the scattering between the camera and the point at
    // distance l, by implementing Eq. (18) of the paper (shadow_transmittance
    // is the T(x,x_s) term, scattering is the S|x_s=x+lv term).
    float d = shadow_length;
    float r_p =
        ClampRadius(planet, sqrt(d * d + 2.0 * r * mu * d + r * r));
    float mu_p = (r * mu + d) / r_p;
    float mu_l_p = (r * mu_l + d * nu) / r_p;

    scattering = GetCombinedScattering(
        planet, singleScatteringTable,
        r_p, mu_p, mu_l_p, nu, ray_r_mu_intersects_ground, lightIndex,
        single_mie_scattering);
    vec3 shadow_transmittance =
        GetTransmittance(planet, transmittanceTable,
            r, mu, shadow_length, ray_r_mu_intersects_ground);
    scattering = scattering * shadow_transmittance;
    single_mie_scattering = single_mie_scattering * shadow_transmittance;
  }
  return (scattering * RayleighPhaseFunction(nu) + single_mie_scattering *
      MiePhaseFunction(planet.mieAsymmetryFactor, nu)) * SkyRadianceToLuminance.rgb;
}

int M_perAtmospherePixel = floatBitsToInt(Multisampling_perAtmospherePixel);;
float raymarchOcclusion(Planet planet, Ray ray, float fromT, float toT, bool viewTerrainHit, DirectionalLight l)
{
	float t = fromT;
	float dx = (toT - fromT) / float(M_perAtmospherePixel);
	dx = max(dx, QualitySettings_minStepSize);// Otherwise too close object would evaluate too much steps
	float shadowLength = 0;
	for(int i = 0; i < M_perAtmospherePixel; i++,t+=dx)
	{
		vec3 worldPos = ray.origin + ray.direction * t;
		vec3 planetRelativePos = worldPos - planet.center;
		float r = length(planetRelativePos);
		vec3 sphNormal = planetRelativePos /r;
		float mu = dot(sphNormal, ray.direction);
		float mu_s = dot(sphNormal, l.direction);
		float nu = dot(ray.direction, l.direction);
		bool noSureIfEclipse = true;
		float lightFromT = 0, lightToT, _;
		/*if(viewTerrainHit)
		{
			if(mu_s < LightSettings_viewThres)
			{
				shadowLength += dx;
				continue;//No light when sun is under the horizon
			}
			// When view ray and sun are on the opposite side, there nearly "should not be" any rays
			float diff = nu - LightSettings_noRayThres;
			// But it whould sometimes create a visible seam, so we create a gradient here
			if(diff < 0 && mod(i, LightSettings_gradient) < 1)
			{
				noSureIfEclipse = false;
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
		*/
		// Intersect light ray with outer shell of the atmosphere
		Ray shadowRay = Ray(worldPos, l.direction);
		raySphereIntersection(planet.center, planet.atmosphereRadius,
							shadowRay, _, lightToT);
							/*
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
		}*/

		float cloudsDensity = raymarchCloudsL(planet, shadowRay, 0, Clouds_occlusionFarPlane, Clouds_occlusionSteps);
		shadowLength += dx * clamp(1 - exp(-cloudsDensity * Clouds_occlusionPower), 0, 1);
	}
	return shadowLength;
}

void precomputedAtmosphere(Planet p, Ray ray, float toT, bool terrainWasHit, inout vec3 luminance, inout vec3 transmittance)
{
	vec3 atmoTransmittance;
	float lightIndex = 0;
	vec3 planetSpaceCam = ray.origin - p.center;
	for(uint l = p.firstLight; l <= p.lastLight; l++)
	{
		DirectionalLight light = directionalLights[l];
		float shadow = HQSettings_lightShafts ? raymarchOcclusion(p, ray, 0, toT, terrainWasHit, light) : 0;
		luminance += GetSkyRadianceToPoint(p, transmittanceTable, singleScatteringTable,
											planetSpaceCam, toT, ray.direction, shadow, 
											light.direction, lightIndex, /*out*/ atmoTransmittance)
					* SkyRadianceToLuminance.rgb * transmittance;
		lightIndex++;
	}
	transmittance *= atmoTransmittance;
}

float raymarchAtmosphere(Planet planet, Ray ray, float minDistance, float maxDistance, inout vec3 luminance, inout vec3 transmittance, bool terrainWasHit);
bool terrainColorAndHit(Planet p, Ray ray, float fromDistance, inout float toDistance, inout vec3 throughput, inout vec3 luminance, out Hit terrainHit)
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
		vec3 planetAlbedo = terrainColor(p, toDistance, worldSamplePos, worldNormal, sampleHeight);
		cloudsForPlanet(p,ray,fromDistance,toDistance,Clouds_terrainSteps,throughput,luminance);
		if(!DEBUG_ATMO_OFF)
		{
			//Compute atmosphere contribution between terrain and camera
			if(HQSettings_atmoCompute)
				raymarchAtmosphere(p, ray, fromDistance, toDistance, /*inout*/ luminance, /*inout*/ throughput, true);
			else
			{
				precomputedAtmosphere(p, ray, toDistance, true, luminance, throughput);
			}
		}
		luminance += planetAlbedo * lightPoint(p, worldSamplePos, worldNormal) * throughput;
		throughput *= planetAlbedo;
		terrainHit = Hit(worldSamplePos, worldNormal, -1, toDistance);
		return true;
	}
	return false;
}

/**
  * Does a raymarching through the atmosphere and planet
  * @returns 0 when only atmosphere or nothing was hit, else a number higher than 0 (when the planet was hit)
  * @param tMax By tweaking the tMax parameter, you can limit the assumed ray length
  * This is useful when the ray was blocked by some objects
  */
bool planetsWithAtmospheres(Ray ray, float tMax/*some object distance*/, out vec3 luminance, inout vec3 throughput, out Hit planetHit)
{
	luminance = vec3(0);
	for (int k = 0; k < planets.length(); ++k)
    {
        Planet p = planets[k];
		float fromDistance, toDistance;
		if(!raySphereIntersection(p.center, p.atmosphereRadius, ray, /*out*/ fromDistance, /*out*/ toDistance)
		|| (toDistance < 0 ))// this would mean that the atmosphere and the planet is behind us
		{
			return false;
		}
		float atmoDistance = toDistance;
        float surfaceDistance, t1; 
		bool surfaceIntersection;
        if (surfaceIntersection = raySphereIntersection(p.center, p.surfaceRadius, ray, surfaceDistance, t1) && t1 > 0) 
        {
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
		cloudsForPlanet(p,ray,fromDistance,toDistance,Clouds_iter,throughput,luminance);
		if(!DEBUG_ATMO_OFF)
		{	
			if(HQSettings_atmoCompute)
				raymarchAtmosphere(p, ray, fromDistance, toDistance, /*inout*/ luminance,/*inout*/ throughput, false);
			else
			{
				precomputedAtmosphere(p, ray, toDistance, false, luminance, throughput);
			}
		}
		
		vec3 worldHitPos = ray.origin + ray.direction * toDistance;
		planetHit = Hit(worldHitPos, normalize(worldHitPos - p.center), -1, surfaceIntersection ? surfaceDistance : POSITIVE_INFINITY);
		return false;
	}
}


float raymarchAtmosphere(Planet planet, Ray ray, float minDistance, float maxDistance, inout vec3 luminance, inout vec3 transmittance, bool terrainWasHit)
{
	float t0, t1;

	float pathFraction = (maxDistance - minDistance) / M_perAtmospherePixel;
	float segmentLength = max(pathFraction, QualitySettings_minStepSize);// Otherwise too close object would evaluate too much steps

	vec3 rayleighColor = vec3(0);
	vec3 mieColor = vec3(0);
	float mieExtinction = 1.11 * planet.mieCoefficient;
	float opticalDepthR = 0, opticalDepthM = 0, opticalDepthO = 0; 

	float currentDistance;
	float i = QualitySettings_minStepSize / pathFraction;//float iterator
	int iter = int(i);//integer iterator
	for(currentDistance = minDistance; iter < M_perAtmospherePixel; currentDistance += segmentLength, iter++, i++)
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

			float lightFromT = 0, dummy, lightToT; 
			// Intersect light ray with outer shell of the planet
			Ray shadowRay = Ray(worldSamplePos, light.direction);
			raySphereIntersection(planet.center, planet.atmosphereRadius,
								shadowRay, dummy, lightToT);

			//Firstly check for object hits
			Hit hit = findObjectHit(shadowRay, false);
			lightToT = min(hit.t, lightToT);
			// Secondly check if sun is in shadow of the planet
			float sunToNormalCos = dot(light.direction, sphNormal);
			vec3 viewOnPlanetPlane = ray.direction - dot(ray.direction,sphNormal) * sphNormal;
			vec3 sunOnPlanetPlane = light.direction - sunToNormalCos * sphNormal;

			bool noSureIfEclipse = true;
			if(terrainWasHit)
			{
				if(sunToNormalCos < LightSettings_viewThres)
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
			if(sunToNormalCos > LightSettings_noRayThres || currentDistance > QualitySettings_farPlane)
			{
				if(DEBUG_RM)
					luminance = vec3(0,1,0);
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
							luminance = vec3(1,0,0);
						// In all the later samples, the sun will be also occluded
						break;//The later samples would all be occluded by the terrain
					}
					continue;//Skip to next sample. This effectively creates light rays
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