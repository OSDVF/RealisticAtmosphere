//?#version 440
#ifndef CLOUDS_H
#define CLOUDS_H
#include "Random.glsl"
#include "Hit.glsl"
#include "Intersections.glsl"
uniform sampler2D cloudsMieLUT;

float cloudsMediumPrec(vec3 x) {
	float level1 = 0.0;
    float v = 0.0;
	float a = 0.5;
	vec3 shift = vec3(100);
    level1 = noise(x);
	x = x * 2.0 + shift;
	for (int i = 0; i < 3; ++i) {
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
    level1 = noise(x);
	x = x * 2.0 + shift;
	for (int i = 0; i < 6; ++i) {
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
	return noise(x);
}

vec3 miePhaseFunction(float cosPhi)
{
    float angle = acos(cosPhi);
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

float powderTerm(float density, float cosTheta) {
    float powder = 1.0 - exp(-density*HQSettings_cloudsDensity * 2.0);
    powder = clamp(powder * 2.0, 0.0, 1.0);
    return mix(1.0, powder, smoothstep(0.5, -0.5, cosTheta));
}

float heightFade(float cloudDensity, Planet p, vec3 worldPos)
{
    float height = distance(worldPos, p.center);
    cloudDensity = mix(0, cloudDensity, pow(1-clamp((height - p.cloudsEndRadius + Clouds_atmoFade)/Clouds_atmoFade, 0, 1), 2));
    cloudDensity = mix(0, cloudDensity, pow(clamp((height - p.cloudsStartRadius)/Clouds_terrainFade, 0, 1), 2));
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

vec3 sampleLight(Planet planet, Ray ray, vec3 original_sample_point, float original_sample_density, vec3 transmittance) {
    vec3 sun_direction = directionalLights[planet.sunDrectionalLightIndex].direction.xyz;
    float nu = dot(sun_direction, ray.direction);

    float dummy, toDistance, toDistanceInner;
    Ray shadowRay = Ray(original_sample_point, sun_direction);
    raySphereIntersection(planet.center, planet.cloudsEndRadius, shadowRay, /*out*/ dummy, /*out*/ toDistance);
    raySphereIntersection(planet.center, planet.cloudsStartRadius, shadowRay, /*out*/ toDistanceInner, /*out*/ dummy);
    if(toDistanceInner > 0)
        toDistance = min(toDistance, toDistanceInner);
    /*if(!raySphereIntersection(ray.origin, Clouds_farPlane, shadowRay, dummy,  toDistanceInner))
    {
        return vec3(1);
    }
    if(toDistanceInner > 0)
        toDistance = min(toDistance, toDistanceInner);*/

    vec3 ray_step = sun_direction * (min(toDistance, Clouds_lightFarPlane)/Clouds_lightSteps);
    vec3 sample_point = original_sample_point + ray_step;

    float thickness = 0.0;

    for(float i = 0.0; i < Clouds_lightSteps; i++) {
        sample_point += ray_step;
        thickness += sampleCloudM(planet, sample_point);
    }

    sample_point += ray_step * 8.0;
    thickness += sampleCloudM(planet, sample_point);

    vec3 phase = miePhaseFunction(nu);

    vec3 clouds_position = original_sample_point;

    vec3 point = clouds_position;
    //float r = length(point);
    //vec3 sun_dir = sun_direction;
    //float mu_s = dot(point, sun_dir) / r;
    vec3 clouds_sky_illuminance = vec3(0);//SkyRadianceToLuminance.xyz * get_irradiance(globals.parameters, irradiance_texture, r, mu_s);

    vec3 clouds_sun_illuminance = SunRadianceToLuminance.xyz * planet.sunIntensity;//globals.parameters.solar_irradiance
        // *get_transmittance_to_sun(globals.parameters, transmittance_texture, r, mu_s);

    vec3 clouds_luminance = (1.0 / (pi * pi)) * (clouds_sun_illuminance + clouds_sky_illuminance);

    vec3 clouds_transmittance = transmittance;
    vec3 clouds_in_scatter = vec3(0);//get_sky_luminance_to_point(ray.origin, clouds_position, sun_direction, clouds_transmittance, transmittance_texture, scattering_texture);
    
    vec3 color = clouds_luminance * clouds_transmittance + clouds_in_scatter;

    /*if (color.b != 0.0) {
        color *= (color.r * color.g) / color.b;
    }*/
    float beer = exp(-thickness*HQSettings_cloudsDensity);

    color = color * phase * beer * powderTerm(original_sample_density, nu);
    vec3 planetNormal = normalize(original_sample_density - planet.center);
    color += max(
                dot(planetNormal, sun_direction),
                0.0
                ) 
            *
            mix(
                vec3(0.507,0.754,1.0),
                vec3(1.0),
                normalized_altitude(planet, original_sample_point)
            )
        ;
        
    //return max(1.0 - textureLod(coverage_texture, original_sample_point.xz * coverage_scale * 0.5 + 0.5 + coverage_offset, 0.0).g, 0.05) * color;
    return color;
}

void raymarchClouds(Planet planet, Ray ray, float fromT, float toT, inout vec3 transmittance, inout vec3 radiance)
{
	float t = fromT;
	float segmentLength = (toT - fromT) / Clouds_iter;
    vec4 finalColor = vec4(0);
    int iter = int(Clouds_iter);
	for(int i = 0; i < iter ; i++)
	{
        if(t > toT)
            break;
		vec3 worldSpacePos = ray.origin + ray.direction * t;
        float cheapDensity = sampleCloudCheap(planet, worldSpacePos);

        #if 1
        if(cheapDensity > Clouds_cheapThreshold)
        {
            t -= segmentLength * Clouds_cheapDownsample * Clouds_backStep; //Return to the previous sample because we could lose some cloud material
            float density = sampleCloud(planet, worldSpacePos, cheapDensity);
            do
            {
                vec3 worldSpacePos = ray.origin + ray.direction * t;
                if(density > 0)
                {
                    //finalColor.xyz += vec3(density);
                    vec4 particle = vec4(density);
                    float sample_transmittance = 1.0 - particle.a;
                    transmittance *= sample_transmittance;            

                    particle.a = 1.0 - sample_transmittance;
                    particle.rgb = sampleLight(planet, ray, worldSpacePos, particle.a, transmittance) * particle.a;

                    finalColor += (1.0 - finalColor.a) * particle;
                }


                if(finalColor.a>=1.0)
                    break;

                density = sampleCloudH(planet, worldSpacePos, cheapDensity);
                t += segmentLength;
                i++;
            }
            while(i < iter && cheapDensity > Clouds_cheapThreshold);
        }
        else
        {
            t += segmentLength * Clouds_cheapDownsample;
        }
        #endif
	}
    radiance += finalColor.xyz;
}

void cloudsForPlanet(Planet p, Ray ray, float fromDistance, float toDistance, inout vec3 transmittance, inout vec3 radiance)
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
		raymarchClouds(p, ray, fromDistance, toDistance, transmittance, radiance);
	}
}
#endif