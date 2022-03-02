//?#version 440
#ifndef LIGHTING_H
#define LIGHTING_H

#include "Buffers.glsl"
#include "Intersections.glsl"
#include "Common.glsl"

vec3 sunAndSkyIlluminance(Planet planet,vec3 point, vec3 normal, vec3 sun_direction)
{
    vec3 toPlanetSpace = point - planet.center;
    float r = length(toPlanetSpace);
    float mu_s = dot(toPlanetSpace, sun_direction) / r;
    return /*sky*/
        /*GetIrradiance(planet, irradianceTable, r, mu_s) *
        (1.0 + dot(normal, toPlanetSpace) / r) * 0.5 *
        SkyRadianceToLuminance.xyz +*/ // Indirect irradiance precomputation not yet implemented. The table contains direct irradiance
        /*sun*/
        planet.solarIrradiance *
        GetTransmittanceToSun(planet, transmittanceTable ,r, mu_s) *
        max(dot(normal, sun_direction), 0.0) *
        SunRadianceToLuminance.xyz * planet.sunIntensity;
}

vec3 sunAndSkyIlluminance(Planet planet,vec3 point, vec3 sun_direction)
{
    vec3 toPlanetSpace = point - planet.center;
    float r = length(toPlanetSpace);
    float mu_s = dot(toPlanetSpace, sun_direction) / r;
    return /*sky*/
        /*GetIrradiance(planet, irradianceTable, r, mu_s) *
        (1.0 + dot(normal, toPlanetSpace) / r) * 0.5 *
        SkyRadianceToLuminance.xyz +*/ // Indirect irradiance precomputation not yet implemented. The table contains direct irradiance
        /*sun*/
        planet.solarIrradiance *
        GetTransmittanceToSun(planet, transmittanceTable ,r, mu_s) *
        SunRadianceToLuminance.xyz * planet.sunIntensity;
}

vec3 lightPoint(Planet planet, vec3 p, vec3 normal)
{
    vec3 totalLightColor = AMBIENT_LIGHT;// Initially the object is only lightened up by ambient light
    // Compute illumination by casting 'shadow rays' into lights

    // Check for directional lights
    for(int i = 0; i < directionalLights.length(); i++)
    {
        DirectionalLight light = directionalLights[i];
        vec3 lDir = light.direction.xyz;
        bool inShadow = false;
        Ray shadowRay = Ray(p, lDir);
        for (int k = 1; k < objects.length(); ++k) {
            float t0 = 0, t1 = 0;
            if (raySphereIntersection(objects[k].position, objects[k].radius, shadowRay, t0, t1) && t1 > 0) {
                return AMBIENT_LIGHT;
            }
        }
        if(planet.sunDrectionalLightIndex == i)
        {
            totalLightColor += light.color.xyz * sunAndSkyIlluminance(planet, p, normal, lDir);
        }
        else
        {
            totalLightColor += light.color.xyz * max(dot(normal, lDir),0);
        }
    }
    return totalLightColor;
}

vec3 computeLightColor(Hit hit)
{
    if(DEBUG_NORMALS)
    {
        return hit.normalAtHit;
    }
    vec3 totalLightColor = AMBIENT_LIGHT;// Initially the object is only lightened up by ambient light
    // Compute illumination by casting 'shadow rays' into lights

    // Check for directional lights
    for(int i = 0; i < directionalLights.length(); i++)
    {
        DirectionalLight light = directionalLights[i];
        vec3 lDir = light.direction.xyz;
        bool inShadow = false;
        Ray shadowRay = Ray(hit.position, lDir);
        for (int k = 1; k < objects.length(); ++k) {
            if(k == hit.hitObjectIndex)
            {
                continue;
            }
            float t0 = 0, t1 = 0;
            if (raySphereIntersection(objects[k].position, objects[k].radius, shadowRay, t0, t1) && t1 > 0) {
                return AMBIENT_LIGHT;
            }
        }
        /*for(int k = 0; k < planets.length();++k)
        {
            if(intersectsPlanet(planets[k], shadowRay))
            {
                return vec3(0,0,1);
            }
        }*/
        totalLightColor += light.color.xyz * max(dot(lDir, hit.normalAtHit),0);
    }
    return totalLightColor;
}
#endif