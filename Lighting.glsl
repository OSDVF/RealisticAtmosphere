//?#version 440
#include "Buffers.glsl"
#include "Intersections.glsl"

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
        for (int k = 0; k < objects.length(); ++k) {
            if(k == hit.hitObjectIndex||k==0)
            {
                continue;
            }
            float t0 = 0, t1 = 0;
            if (raySphereIntersection(objects[k].position, objects[k].radius, shadowRay, t0, t1)) {
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
        totalLightColor += light.color.xyz * dot(lDir, hit.normalAtHit);
    }
    return totalLightColor;
}