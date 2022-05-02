//?#version 440
#ifndef LIGHTING_H
#define LIGHTING_H

#include "Buffers.glsl"
#include "Intersections.glsl"
#include "Common.glsl"
#include "Terrain.glsl"

vec3 PlanetIlluminance(Planet planet, vec3 point, vec3 phaseFunctionValue)
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

vec3 PlanetIlluminance(Planet planet, vec3 point)
{
    return PlanetIlluminance(planet, point, vec3(1));
}

vec3 lightPoint(Planet planet, vec3 p, vec3 normal, out bool shadowedByTerrain)
{
    shadowedByTerrain = false;
    vec3 totalLightColor = AMBIENT_LIGHT;// Initially the object is only lightened up by ambient light
    // Compute illumination by casting 'shadow rays' into lights

    // Check for directional lights
    float lightIndexInTexture = planet.firstLightCoord;
    vec3 planetRelativePos = p - planet.center;
    float r = length(planetRelativePos);
    vec3 sphNormal = planetRelativePos / r;
    for(uint i = planet.firstLight; i <= planet.lastLight; i++,lightIndexInTexture++)
    {
        DirectionalLight light = directionalLights[i];
        vec3 lDir = light.direction.xyz;
        bool inShadow = false;
        Ray shadowRay = Ray(p, lDir);
        for (int k = 0; k < objects.length(); ++k) {
            float t0 = 0, t1 = 0;
            if (materials[objects[k].materialIndex].albedo.a > 0 && raySphereIntersection(objects[k].position, objects[k].radius, shadowRay, t0, t1) && t1 > 0) {
                inShadow = true;
                break;
            }
        }
        if(HQSettings_earthShadows && !inShadow)
        {
            for(int k = 0; k < planets.length();++k)
            {
                if(raymarchTerrainD(planets[k], shadowRay, LightSettings_shadowNearPlane/*offset a bit to reduce self-shadowing*/, LightSettings_farPlane))
                {
                    inShadow = true;
                    shadowedByTerrain = true;
                }
            }
        }
        float mu_l = dot(sphNormal, light.direction);
        vec3 thisLightColor = HQSettings_indirectApprox /*use precomputed indirect lighting?*/ ? /*initially illuminated only by sky*/
                            (GetIrradiance(planet, irradianceTable, r, mu_l, lightIndexInTexture) *
                            (1.0 + dot(normal, sphNormal)) * 0.5 *
                            SkyRadianceToLuminance.xyz) : vec3(0);
        if(!inShadow)
        {
            thisLightColor += light.irradiance *
                            GetTransmittanceToLight(planet, light.angularRadius, transmittanceTable, r, mu_l) *
                            max(dot(normal, light.direction), 0.0) *
                            SunRadianceToLuminance.xyz;       
        }
        totalLightColor += thisLightColor * light.intensity;
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
        vec3 lDir = light.direction;
        bool inShadow = false;
        Ray shadowRay = Ray(hit.position, lDir);
        for (int k = 0; k < objects.length(); ++k) {
            if(k == hit.hitObjectIndex || materials[objects[k].materialIndex].albedo.a == 0)
            {
                continue;
            }
            float t0 = 0, t1 = 0;
            if (raySphereIntersection(objects[k].position, objects[k].radius, shadowRay, t0, t1) && t1 > 0) {
                inShadow = true;
                break;
            }
        }
        if(HQSettings_earthShadows && !inShadow)
        {
            for(int k = 0; k < planets.length();++k)
            {
                if(raymarchTerrainL(planets[k], shadowRay, LightSettings_shadowNearPlane, LightSettings_farPlane))
                {
                    inShadow = true;
                }
            }
        }
        if(inShadow)
            continue;
        totalLightColor += light.irradiance * max(dot(lDir, hit.normalAtHit),0);
    }
    return totalLightColor;
}
#endif