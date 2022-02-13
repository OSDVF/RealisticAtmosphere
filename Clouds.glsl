//?#version 440
#ifndef CLOUDS_H
#define CLOUDS_H
#include "Random.glsl"
#include "Hit.glsl"
#include "Intersections.glsl"
uniform sampler2D cloudsMieLUT;

vec3 miePhaseFunction(float cosPhi)
{
    float angle = acos(cosPhi);
	return texture(cloudsMieLUT, vec2(angle/pi,0)).xyz;
}

float powderTerm(float density, float cosTheta) {
    float powder = 1.0 - exp(-density * 2.0);
    powder = clamp(powder * 2.0, 0.0, 1.0);
    return mix(1.0, powder, smoothstep(0.5, -0.5, cosTheta));
}

float sampleCloud(vec3 worldPos)
{
    return clamp(pow(fbm6(worldPos * Clouds_size)+Clouds_density, Clouds_edges), 0, 1);
}

vec3 sampleLight(Planet planet, Ray ray, vec3 original_sample_point, float original_sample_density, vec3 transmittance) {
    vec3 sun_direction = directionalLights[planet.sunDrectionalLightIndex].direction.xyz;
    float nu = dot(sun_direction, ray.direction);
    float fromDistance, toDistance;
    raySphereIntersection(planet.center, planet.cloudRadius, Ray(original_sample_point, sun_direction), /*out*/ fromDistance, /*out*/ toDistance);

    vec3 ray_step = sun_direction * (toDistance/Clouds_iter);
    vec3 sample_point = original_sample_point + ray_step;

    float thickness = 0.0;

    for(float i = 0.0; i < Clouds_iter; i++) {
        sample_point += ray_step;
        thickness += sampleCloud(sample_point);
    }

    /*sample_point += ray_step * 8.0;
    thickness += sampleCloud(sample_point);*/

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

    if (color.b != 0.0) {
        color *= (color.r * color.g) / color.b;
    }
    float beer = exp(thickness);

    color = color * phase * beer * powderTerm(original_sample_density, nu);
    /*color += max(dot(vec3(0.0, 1.0, 0.0), sun_direction), 0.0) 
            * mix(vec3(0.507,0.754,1.0), vec3(1.0)
        , normalized_altitude(original_sample_point))
        ;*/

    //return max(1.0 - textureLod(coverage_texture, original_sample_point.xz * coverage_scale * 0.5 + 0.5 + coverage_offset, 0.0).g, 0.05) * color;
    return color;
}

void raymarchClouds(Planet planet, Ray ray, float fromT, float toT, inout vec3 transmittance, inout vec3 radiance)
{
	float t;
	float segmentLength = (toT - fromT) / Clouds_iter;
    vec4 finalColor = vec4(0);
	for(t = fromT; t<= toT; t+= segmentLength)
	{
		vec3 worldSpacePos = ray.origin + ray.direction * t;
		float density = sampleCloud(worldSpacePos);
        if(density > 0.0)
        {
            finalColor.xyz += vec3(density);
            //vec4 particle = vec4(density);
            //float sample_transmittance = 1.0 - particle.a;
            //transmittance *= sample_transmittance;            

            //particle.a = 1.0 - sample_transmittance;
            //particle.rgb = sampleLight(planet, ray, worldSpacePos, particle.a, transmittance) * particle.a;

            //finalColor += (1.0 - finalColor.a) * particle;
        }

        //if(finalColor.a>=1.0)
          //  break;
	}
    radiance += finalColor.xyz;
}

void cloudsForPlanet(Planet p, Ray ray, float fromDistance, float toDistance, inout vec3 transmittance, inout vec3 radiance)
{
    float t0,t1;
    if(raySphereIntersection(p.center, p.cloudRadius, ray, t0, t1) && t1>0)
	{
		raymarchClouds(p, ray, fromDistance, min(t1, toDistance), transmittance, radiance);
	}
}
#endif