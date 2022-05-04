//?#version 440
#ifndef CLOUDS_H
#define CLOUDS_H
#include "Random.glsl"
#include "Hit.glsl"
#include "Intersections.glsl"
#include "Lighting.glsl"
uniform sampler2D cloudsMieLUT;

float Coverage(CloudLayer c, vec3 x)
{
    return 1.0 - micro(x * c.coverage);
}

float cloudsMediumPrec(CloudLayer c, vec3 x) {
	float level1 = 0.0;
    float v = 0.0;
	float a = 0.5;
    level1 = Coverage(c,x);
	for (int i = 0; i < 3; ++i) {
		v += a * Value3D(x);
		x = x * 2.0;
		a *= 0.5;
	}
	return level1 - v;
}

float cloudsMediumPrec(CloudLayer c, vec3 x, out float level1) {
	level1 = 0.0;
    float v = 0.0;
	float a = 0.5;
    level1 = Coverage(c,x);
	for (int i = 0; i < 3; ++i) {
		v += a * Value3D(x);
		x = x * 2.0;
		a *= 0.5;
	}
	return level1 - v;
}

float cloudsHighPrec(CloudLayer c, vec3 x, out float level1) {
	level1 = 0.0;
    float v = 0.0;
	float a = 0.5;
    level1 = Coverage(c,x);
	for (int i = 0; i < 6; ++i) {
		v += a * Value3D(x);
		x = x * 2.0;
		a *= 0.5;
	}
	return level1 - v;
}

float cloudsHigherOrders(vec3 x)
{
    float v = 0.0;
	float a = 0.5;
	for (int i = 0; i < 6; ++i) {
		v += a * Value3D(x);
		x = x * 2.0;
		a *= 0.5;
	}
	return v;
}

float cloudsCheap(CloudLayer c, vec3 x) {
	return Coverage(c,x);
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

float powder(float density, float nu) {
    float powderApprox = 1.0 - exp(density * Clouds_powderDensity);
    return clamp(powderApprox * nu, Clouds_powderAmbient, 1);
}

float heightFade(float cloudDensity, CloudLayer c, float height)
{
    cloudDensity = mix(0, cloudDensity, pow(1-clamp((height - c.endRadius + c.upperGradient)/c.upperGradient, 0, 1), Clouds_fadePower));
    cloudDensity = mix(0, cloudDensity, pow(clamp((height - c.startRadius)/c.lowerGradient, 0, 1), Clouds_fadePower));
    return cloudDensity;
}

float sampleCloudM(CloudLayer c, vec3 cloudSpacePos, float height)
{
    float cloudDensity = pow(clamp(cloudsMediumPrec(c, cloudSpacePos * c.sizeMultiplier) * c.density, 0.0, 1.0), c.sharpness);
    return heightFade(cloudDensity, c, height);
}
float sampleCloudL(CloudLayer c, vec3 cloudSpacePos, float height, out float cheapDensity)
{
    float cloudDensity = pow(clamp(cloudsMediumPrec(c, cloudSpacePos * c.sizeMultiplier, cheapDensity) * c.density, 0.0, 1.0), c.sharpness);
    return heightFade(cloudDensity, c, height);
}
float sampleCloudH(CloudLayer c, vec3 cloudSpacePos, float height, out float cheapDensity)
{
    float cloudDensity = pow(clamp(cloudsHighPrec(c, cloudSpacePos * c.sizeMultiplier, cheapDensity) * c.density, 0.0, 1.0), c.sharpness);
    return heightFade(cloudDensity, c, height);
}
float sampleCloud(CloudLayer c, vec3 cloudSpacePos, float height, float level1)
{
    float cloudDensity = pow(clamp((level1 - cloudsHigherOrders(cloudSpacePos * c.sizeMultiplier)) * c.density, 0.0, 1.0), c.sharpness);
    return heightFade(cloudDensity, c, height);
}
float sampleCloudCheap(CloudLayer c, vec3 cloudSpacePos)
{
    return clamp(cloudsCheap(c, cloudSpacePos * c.sizeMultiplier),0,1);
}

float raymarchClouds(Planet planet, Ray ray, float fromT, float toT, float steps, out vec3 transmittance, out vec3 luminance)
{
    transmittance = vec3(1);
    luminance = vec3(0);
    float placeWithMaxDensity = 0.0;
    float maxDensity = 0.0;

	float t = fromT;
	float segmentLength = (toT - fromT) / steps;
    int iter = int(steps);
    
    if(HQSettings_reduceBanding)
    //Reduce banding by offseting origin about random fraction
    {
        ray.origin += ray.direction * debandingNoise(ray.direction + time.x * HQSettings_sampleNum, LightSettings_deBanding);
    }

    for(uint l = planet.firstLight; l <= planet.lastLight; l++)
    {
        vec3 sunDir = directionalLights[l].direction;
        float nu = dot(sunDir, ray.direction);
        vec3 phase = combinedPhaseFunction(nu);
        CloudLayer c = planet.clouds;
	    for(int i = 0; i < iter ; i++)
	    {
            if(t > toT)
                break;
            vec3 worldSpacePos = ray.origin + ray.direction * t;
		    vec3 cloudSpacePos = worldSpacePos + c.position;
            float cheapDensity = sampleCloudCheap(c, cloudSpacePos);
            
            if(cheapDensity > Clouds_cheapThreshold)
            {
                if(i > 0) t -= segmentLength * Clouds_cheapDownsample*.8; //Return to the previo  us sample because we could lose some cloud material
                worldSpacePos = ray.origin + ray.direction * t;
                cloudSpacePos = worldSpacePos + c.position;
                float height = distance(worldSpacePos, planet.center);
                float density = min(sampleCloud(c, cloudSpacePos, height, cheapDensity), 1);

                vec3 scatteringSum = vec3(0);
                do
                {
                    if(density > Clouds_sampleThres)
                    {
                        if(density > maxDensity)
                        {
                            maxDensity = density;
                            placeWithMaxDensity = t;
                        }

                        // Shadow rays:
                        float lOpticalDepth = 0;
                        Ray shadowRay = Ray(worldSpacePos, sunDir);
                        float dummy, lToDistance, toDistanceInner;
                        raySphereIntersection(planet.center, c.endRadius, shadowRay, /*out*/ dummy, /*out*/ lToDistance);
                        raySphereIntersection(planet.center, c.startRadius, shadowRay, /*out*/ toDistanceInner, /*out*/ dummy);
                        if(toDistanceInner > 0)
                            lToDistance = min(lToDistance, toDistanceInner);

                        float lSegmentLength = min(lToDistance,Clouds_lightFarPlane)/Clouds_lightSteps;
                        vec3 lightRayStep = sunDir * lSegmentLength;
                        vec3 lCloudSamplePos = cloudSpacePos;
                        vec3 lWorldSamplePos = worldSpacePos;
                        float coneOffset = 1;
                        for(float s = 0; s < Clouds_lightSteps; s++)
                        {
                            float lheight = distance(lWorldSamplePos, planet.center);
                            float lDensity = min(sampleCloudM(c, lCloudSamplePos, lheight), 1);
                            lOpticalDepth += lDensity * lSegmentLength;
                            if(s == Clouds_lightSteps - 2)
                            {
                                lightRayStep *= 3;
                                lSegmentLength *= 3;
                            }
                            if(HQSettings_reduceBanding)
                            {
                                //Light samples are scattered inside a cone
                                vec3 stepWithDeBanding = vec3(
                                    random(lCloudSamplePos.x + time.x * HQSettings_sampleNum),
                                    random(lCloudSamplePos.y + time.x * HQSettings_sampleNum),
                                    random(lCloudSamplePos.z + time.x * HQSettings_sampleNum)
                                    ) * Clouds_deBanding * coneOffset + lightRayStep;
                                coneOffset *= Clouds_cone;
                                lCloudSamplePos += stepWithDeBanding;
                                lWorldSamplePos += stepWithDeBanding;
                            }
                            else
                            {
                                lCloudSamplePos += lightRayStep;
                                lWorldSamplePos += lightRayStep;
                            }
                        }
                    
                        // Single scattering
                        float beer = min(
                                        exp(-lOpticalDepth * c.extinctionCoef) + Clouds_beerAmbient,
                                    1);
                        float opticalDepth = density * segmentLength;

                        // Multiple scattering approximation
                        float pw = powder(lOpticalDepth, nu);

                        transmittance *= exp(-opticalDepth * c.extinctionCoef) * pw;
                        scatteringSum += beer * density * cloudsIlluminance(planet, worldSpacePos, phase) * transmittance;
                        if(transmittance.x < 0.001 && transmittance.y < 0.001 && transmittance.z < 0.001)
                            break;
                    }

                    i++;
                    if(i >= iter || cheapDensity <= Clouds_cheapThreshold)
                        break;

                    t += segmentLength;
                    worldSpacePos = ray.origin + ray.direction * t;
                    cloudSpacePos = worldSpacePos + c.position;
                    height = distance(worldSpacePos, planet.center);
                    density = min(sampleCloudH(c, cloudSpacePos, height, /*out*/ cheapDensity), 1);
                }
                while(true /* real ending condition is in 'if'above */);
            
                luminance += scatteringSum * segmentLength * c.scatteringCoef;
            }
            else
            {
                t += segmentLength * Clouds_cheapDownsample;
            }
	    }
    }
    return placeWithMaxDensity;
}

//Returns accumulated density
float raymarchCloudsL(Planet planet, Ray ray, float fromT, float toT, float steps)
{
    float t = fromT;
	float segmentLength = (toT - fromT) / steps;
    int iter = int(steps);

    
    float accumulatedDensity = 0;
    CloudLayer c = planet.clouds;
	for(int i = 0; i < iter ; i++)
	{
        if(t > toT)
            break;
        vec3 worldSpacePos = ray.origin + ray.direction * t;
		vec3 cloudSpacePos = worldSpacePos + c.position;
        float cheapDensity = sampleCloudCheap(c, cloudSpacePos);
            
        if(cheapDensity > Clouds_cheapThreshold)
        {
            t -= segmentLength * Clouds_cheapDownsample; //Return to the previous sample because we could lose some cloud material
            worldSpacePos = ray.origin + ray.direction * t;
            cloudSpacePos = worldSpacePos + c.position;
            float height = distance(worldSpacePos, planet.center);
            float density = min(sampleCloudL(c, cloudSpacePos, height, cheapDensity), 1);
            do
            {
                if(density > Clouds_sampleThres)
                {
                    accumulatedDensity+=density * segmentLength;
                }
                if(accumulatedDensity >= Clouds_occlusionMax)
                {
                    return accumulatedDensity;
                }

                worldSpacePos = ray.origin + ray.direction * t;
                cloudSpacePos = worldSpacePos + c.position;
                height = distance(worldSpacePos, planet.center);
                density = min(sampleCloudL(c, cloudSpacePos, height, /*out*/ cheapDensity), 1);
                t += segmentLength;
                i++;
            }
            while(i < iter && cheapDensity > Clouds_cheapThreshold);
        }
        else
        {
            t += segmentLength * Clouds_cheapDownsample;
        }
	}
    return accumulatedDensity;
}

float cloudsForPlanet(Planet p, Ray ray, float fromDistance, float toDistance, float steps, out vec3 transmittance, out vec3 luminance)
{
    transmittance = vec3(1);
    luminance = vec3(0);

    float t0,t1;
    if(raySphereIntersection(p.center, p.clouds.endRadius, ray, t0, t1) && t1>0)
	{
        toDistance = min(min(t1, toDistance),Clouds_farPlane);
        if(raySphereIntersection(p.center, p.clouds.startRadius, ray, t0, t1))
	    {
            if(t0 > 0)
            {
                toDistance = min(t0, toDistance);
            }
            else if(toDistance > t1)
            {
                fromDistance = max(t1, fromDistance);
            }
        }
		return raymarchClouds(p, ray, fromDistance, toDistance, steps, transmittance, luminance);
	}
}
#endif