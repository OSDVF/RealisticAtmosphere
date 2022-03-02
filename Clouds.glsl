//?#version 440
#ifndef CLOUDS_H
#define CLOUDS_H
#include "Random.glsl"
#include "Hit.glsl"
#include "Intersections.glsl"
#include "Lighting.glsl"
uniform sampler2D cloudsMieLUT;

float cloudsMediumPrec(vec3 x) {
	float level1 = 0.0;
    float v = 0.0;
	float a = 0.5;
	vec3 shift = vec3(100);
    level1 = noise(x) * noise(x * Clouds_coverageSize);
	x = x * 2.0 + shift;
	for (int i = 0; i < 2; ++i) {
		v += a * noise(x);
		x = x * 2.0 + shift;
		a *= 0.5;
	}
	return level1 - v;
}

float cloudsHighPrec(vec3 x, out float level1) {
	level1 = 0.0;
    float v = 0.0;
	float a = 0.5;
	vec3 shift = vec3(100);
    level1 = noise(x) * noise(x * Clouds_coverageSize);
	x = x * 2.0 + shift;
	for (int i = 0; i < 5; ++i) {
		v += a * noise(x);
		x = x * 2.0 + shift;
		a *= 0.5;
	}
	return level1 - v;
}

float cloudsHigherOrders(vec3 x)
{
    float v = 0.0;
	float a = 0.5;
	vec3 shift = vec3(100);
	x = x * 2.0;
	for (int i = 0; i < 5; ++i) {
		v += a * noise(x);
		x = x * 2.0 + shift;
		a *= 0.5;
	}
	return v;
}

float cloudsCheap(vec3 x) {
	return noise(x) * noise(x * Clouds_coverageSize);
}

vec3 miePhaseFunction(float cosNu)
{
    float angle = acos(cosNu);
	return texture(cloudsMieLUT, vec2(angle/pi,0)).xyz;
}

// slightly cheaper phase function
float henyey_greenstein_phase_function(float g, float nu) {
    float g2 = g * g;
    return (1.0 - g2) / pow(1.0 + g2 - 2.0 * g * nu, 1.5);
}

const float FORWARD_MIE_SCATTERING_G = 0.8;
const float BACKWARD_MIE_SCATTERING_G = -0.4;

vec3 miePhaseFunctionLow(float cosPhi)
{
    float forward_p = henyey_greenstein_phase_function(FORWARD_MIE_SCATTERING_G, cosPhi);
    float backwards_p = henyey_greenstein_phase_function(BACKWARD_MIE_SCATTERING_G, cosPhi);
    return vec3((forward_p + backwards_p) / 2.0);
}

vec3 combinedPhaseFunction(float cosNu)
{
    return mix(miePhaseFunction(cosNu), vec3(miePhaseFunctionLow(cosNu)), Clouds_aerosols);
}

float powder(float density) {
    float powderApprox = 1.0 - exp(-density * Clouds_powderDensity * 2.0);
    return clamp(powderApprox * 2.0, 0, 1);
}

float heightFade(float cloudDensity, Planet p, vec3 worldPos)
{
    float height = distance(worldPos, p.center);
    cloudDensity = mix(0, cloudDensity, pow(1-clamp((height - p.cloudsEndRadius + Clouds_atmoFade)/Clouds_atmoFade, 0, 1), Clouds_fadePower));
    cloudDensity = mix(0, cloudDensity, pow(clamp((height - p.cloudsStartRadius)/Clouds_terrainFade, 0, 1), Clouds_fadePower));
    return cloudDensity;
}

float sampleCloudM(Planet p, vec3 worldPos)
{
    float cloudDensity = clamp(pow(cloudsMediumPrec(worldPos * Clouds_size) * Clouds_density, Clouds_edges), 0, 1);
    return heightFade(cloudDensity, p, worldPos);
}
float sampleCloudH(Planet p, vec3 worldPos, out float cheapDensity)
{
    float cloudDensity = clamp(pow(cloudsHighPrec(worldPos * Clouds_size, cheapDensity) * Clouds_density, Clouds_edges), 0, 1);
    return heightFade(cloudDensity, p, worldPos);
}
float sampleCloud(Planet p, vec3 worldPos, float level1)
{
    float cloudDensity = clamp(pow((level1 - cloudsHigherOrders(worldPos * Clouds_size)) * Clouds_density, Clouds_edges), 0, 1);
    return heightFade(cloudDensity, p, worldPos);
}
float sampleCloudCheap(Planet p, vec3 worldPos)
{
    return clamp(cloudsCheap(worldPos * Clouds_size),0,1);
}

float normalized_altitude(Planet planet, vec3 position)
{
    return clamp(
            (length(position - planet.center) - planet.cloudsStartRadius)
                            /planet.cloudLayerThickness,
            0,1);
}

void raymarchClouds(Planet planet, Ray ray, float fromT, float toT, float steps, inout vec3 transmittance, inout vec3 radiance)
{
	float t = fromT;
	float segmentLength = (toT - fromT) / steps;
    int iter = int(steps);
    vec3 sunDir = directionalLights[planet.sunDrectionalLightIndex].direction.xyz;
    float nu = dot(sunDir, ray.direction);
    vec3 phase = combinedPhaseFunction(nu);
	for(int i = 0; i < iter ; i++)
	{
        if(t > toT)
            break;
		vec3 worldSpacePos = ray.origin + ray.direction * t + Clouds_position;
        float cheapDensity = sampleCloudCheap(planet, worldSpacePos);

        if(cheapDensity > Clouds_cheapThreshold)
        {
            t -= segmentLength * Clouds_cheapDownsample; //Return to the previo  us sample because we could lose some cloud material
            worldSpacePos = ray.origin + ray.direction * t + Clouds_position;
            float density = min(sampleCloud(planet, worldSpacePos, cheapDensity), 1);
            vec3 finalColor = vec3(0);
            do
            {
                if(density > 0)
                {
                    //finalColor.xyz += vec3(density);
                    float opticalDepth = density * segmentLength;

                    // Shadow rays:
                    float lOpticalDepth = 0;
                    Ray shadowRay = Ray(worldSpacePos, sunDir);
                    float dummy, toDistance, toDistanceInner;
                    raySphereIntersection(planet.center, planet.cloudsEndRadius, shadowRay, /*out*/ dummy, /*out*/ toDistance);
                    raySphereIntersection(planet.center, planet.cloudsStartRadius, shadowRay, /*out*/ toDistanceInner, /*out*/ dummy);
                    if(toDistanceInner > 0)
                        toDistance = min(toDistance, toDistanceInner);

                    float lSegmentLength = min(toDistance,Clouds_lightFarPlane)/Clouds_lightSteps;
                    vec3 lightRayStep = sunDir * lSegmentLength;
                    vec3 lSamplePos = worldSpacePos;
                    for(float s = 0; s < Clouds_lightSteps; s++)
                    {
                        float lDensity = min(sampleCloudM(planet, lSamplePos), 1);
                        lOpticalDepth += lDensity * lSegmentLength;
                        if(s == Clouds_lightSteps - 2)
                        {
                            lightRayStep *= 8;
                            lSegmentLength *= 8;
                        }
                        lSamplePos += lightRayStep;
                    }
                    
                    // Single scattering
                    float beer = exp(-lOpticalDepth * Clouds_extinctCoef);
                    transmittance *= exp(-opticalDepth * Clouds_extinctCoef);
                    vec3 sampleColor = beer * density * segmentLength * Clouds_scatCoef * phase * sunAndSkyIlluminance(planet, worldSpacePos, sunDir) * transmittance;

                    finalColor += sampleColor;
                    if(transmittance.x < 0.001 && transmittance.y < 0.001 && transmittance.z < 0.001)
                        break;
                }

                worldSpacePos = ray.origin + ray.direction * t + Clouds_position;
                density = min(sampleCloudH(planet, worldSpacePos, /*out*/ cheapDensity), 1);
                t += segmentLength;
                segmentLength *= Clouds_cheapCoef;
                i++;
            }
            while(i < iter && cheapDensity > Clouds_cheapThreshold);
            
            radiance += finalColor;
        }
        else
        {
            t += segmentLength * Clouds_cheapDownsample;
            segmentLength *= Clouds_cheapCoef;
        }
	}
}

void cloudsForPlanet(Planet p, Ray ray, float fromDistance, float toDistance, float steps, inout vec3 transmittance, inout vec3 radiance)
{
    float t0,t1;
    if(raySphereIntersection(p.center, p.cloudsEndRadius, ray, t0, t1) && t1>0)
	{
        toDistance = min(min(t1, toDistance),Clouds_farPlane);
        if(raySphereIntersection(p.center, p.cloudsStartRadius, ray, t0, t1))
	    {
            if(t0 > 0)
            {
                toDistance = min(t0, toDistance);
            }
            /*else if(toDistance > t1)
            {
                fromDistance = max(t1, fromDistance);
            }
            else
            {
                return;
            }*/
        }
		raymarchClouds(p, ray, fromDistance, toDistance, steps, transmittance, radiance);
	}
}
#endif