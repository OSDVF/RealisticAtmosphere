/**
 * @author Ond�ej Sabela
 * @brief Realistic Atmosphere - Thesis implementation.
 * @date 2021-2022
 * Copyright 2022 Ond�ej Sabela. All rights reserved.
 * Uses ray tracing, path tracing and ray marching to create visually plausible outdoor scenes with atmosphere, terrain, clouds and analytical objects.
 * skyIndirect, planetLightDirect, cloudsIlluminance and planetIlluminance functions use concepts and code from Precomputed Atmospheric Scattering
 * paper (https://hal.inria.fr/inria-00288758/en) by Eric Bruneton and Fabrice Neyret. Reference implementation at https://github.com/ebruneton/precomputed_atmospheric_scattering.
 */

//?#version 440
#ifndef LIGHTING_H
#define LIGHTING_H

#include "Buffers.glsl"
#include "Intersections.glsl"
#include "Common.glsl"
#include "Terrain.glsl"

vec3 skyIndirect(Planet planet, vec3 normal, vec3 sphNormal, float r, float mu_l, float lightIndexInTexture)
{
    return (GetIrradiance(planet, irradianceTable, r, mu_l, lightIndexInTexture) *
            (1.0 + dot(normal, sphNormal)) * 0.5 *
            SkyRadianceToLuminance.xyz);
}

vec3 planetLightDirect(Planet planet, DirectionalLight light, float r, float mu_l, vec3 normal)
{
    return light.irradiance * GetTransmittanceToLight(planet, light.angularRadius, transmittanceTable, r, mu_l) *
    max(dot(normal, light.direction), 0.0) *
    SunRadianceToLuminance.xyz;
}

vec3 cloudsIlluminance(Planet planet, vec3 point, vec3 phaseFunctionValue)
{
    vec3 toPlanetSpace = point - planet.center;
    float r = length(toPlanetSpace);
    vec3 irradiance = vec3(0);
    float lightTextureIndex = planet.firstLightCoord;
    for(uint l = planet.firstLight; l <= planet.lastLight; l++)
    {
        DirectionalLight light = directionalLights[l];
        float mu_l = dot(toPlanetSpace, light.direction) / r;
        irradiance += light.irradiance * GetTransmittanceToLight(planet, light.angularRadius, transmittanceTable, r, mu_l) * SunRadianceToLuminance.xyz * phaseFunctionValue
        + /*sky*/
        GetIrradiance(planet, irradianceTable, r, mu_l, lightTextureIndex) * SkyRadianceToLuminance.xyz;

        lightTextureIndex++;
    }
    return irradiance;
}

vec3 cloudsIlluminance(Planet planet, vec3 point)
{
    return cloudsIlluminance(planet, point, vec3(1));
}

vec3 planetIlluminance(Planet planet, Hit hit, out bool shadowedByTerrain)
{
    shadowedByTerrain = false;
    vec3 totalLightColor = AMBIENT_LIGHT;// Initially the object is only lightened up by ambient light
    // Compute illumination by casting 'shadow rays' into lights

    // Check for directional lights
    float lightIndexInTexture = planet.firstLightCoord;
    vec3 planetRelativePos = hit.position - planet.center;
    float r = length(planetRelativePos);
    vec3 sphNormal = planetRelativePos / r;
    for(uint i = planet.firstLight; i <= planet.lastLight; i++,lightIndexInTexture++)
    {
        DirectionalLight light = directionalLights[i];
        vec3 lDir = light.direction.xyz;
        bool inShadow = false;
        Ray shadowRay = Ray(hit.position, lDir);
        for (int k = 0; k < objects.length(); ++k) {
            float t0 = 0, t1 = 0;
            if (materials[objects[k].materialIndex].albedo.a > 0 && rayObjectIntersection(objects[k], shadowRay, t0, t1) && t1 > 0) {
                inShadow = true;
                break;
            }
        }
        float softShadow = 1.0;

        shadowRay.origin += LightSettings_secondaryOffset * hit.t * hit.normalAtHit;
        if(HQSettings_earthShadows && !inShadow)
        {
            for(int k = 0; k < planets.length();++k)
            {
                softShadow = raymarchTerrainD(planets[k], shadowRay, hit.t, LightSettings_farPlane);
                if(softShadow == 0.0)
                {
                    inShadow = true;
                    shadowedByTerrain = true;
                }
            }
        }
        float mu_l = dot(sphNormal, light.direction);

        // Apply indirect lighting
        vec3 thisPlanetLightColor = HQSettings_indirectApprox /*use precomputed indirect lighting?*/ ? /*initially illuminated only by sky*/
                            skyIndirect(planet, hit.normalAtHit, sphNormal, r, mu_l, lightIndexInTexture) : vec3(0);
        if(!inShadow)
        {
            thisPlanetLightColor += planetLightDirect(planet, light, r, mu_l, hit.normalAtHit) * softShadow;
        }
        totalLightColor += thisPlanetLightColor;
    }
    return totalLightColor * 0.88;//Terrain textures are too shiny to me, so there is a compensation
}

vec3 objectIlluminance(Hit hit)
{
    if(DEBUG_NORMALS)
    {
        return hit.normalAtHit;
    }
    vec3 totalLightColor = AMBIENT_LIGHT;// Initially the object is only lightened up by ambient light
    // Compute illumination by casting 'shadow rays' into lights

    // TODO optimize for more planets
    float lightIndexInTexture = 0.0;
    vec3 sphericalPos = hit.position - planets[0].center;
    float r = length(sphericalPos);
    vec3 sphNormal = sphericalPos / r;
    // Check for directional lights
    for(int i = 0; i < directionalLights.length(); i++)
    {
        DirectionalLight light = directionalLights[i];
        vec3 lDir = light.direction;
        bool inShadow = false;

        float mu_l = dot(sphNormal, lDir);
        Ray shadowRay = Ray(hit.position, lDir);
        for (int k = 0; k < objects.length(); ++k) {
            if(k == hit.hitObjectIndex || materials[objects[k].materialIndex].albedo.a /*ghost objects don't have shadows*/ == 0)
            {
                continue;
            }
            float t0 = 0, t1 = 0;
            if (rayObjectIntersection(objects[k], shadowRay, t0, t1) && t1 > 0) {
                inShadow = true;
                break;
            }
        }
        float softShadow = 1.0;

        // Prevent calculation from a point iside earth
        shadowRay.origin += LightSettings_secondaryOffset * hit.t * hit.normalAtHit;
        if(HQSettings_earthShadows && !inShadow)
        {
            for(int k = 0; k < planets.length();++k)
            {
                softShadow = raymarchTerrainD(planets[k], shadowRay,hit.t, LightSettings_farPlane);
                if(softShadow == 0.0)
                {
                    inShadow = true;
                }
            }
        }

        if(i >= planets[0].firstLight && i <= planets[0].lastLight)
        {
            // This is a 'planetary' light - sun, moon, etc. and has its light irradiance precomputed in LUT
            // Apply indirect lighting approximation
            vec3 thisPlanetLightColor = HQSettings_indirectApprox /*use precomputed indirect lighting?*/ ? /*initially illuminated only by sky*/
                                skyIndirect(planets[0], hit.normalAtHit, sphNormal, r, mu_l, lightIndexInTexture) : vec3(0);
            if(!inShadow)
            {
                thisPlanetLightColor += planetLightDirect(planets[0], light, r, mu_l, hit.normalAtHit) * softShadow;      
            }
            totalLightColor = thisPlanetLightColor;
            lightIndexInTexture+=1;
        }
        else
        {
            totalLightColor += light.irradiance * max(dot(lDir, hit.normalAtHit),0);
        }
    }
    return totalLightColor;
}
#endif