//?#version 440
#include "Intersections.glsl"
#include "Atmosphere.glsl"
#include "Random.glsl"

#ifdef DEBUG
    uniform vec4 debugAttributes;
    #define DEBUG_NORMALS (debugAttributes.x == 1.0)
#else
    #define DEBUG_NORMALS false
#endif

// Create a primary ray for pixel x, y
Ray createCameraRay(vec2 fromPixel)
{
    float halfWidth = u_viewRect.z / 2.0f;
    float halfHeight = u_viewRect.w / 2.0f;

    // Apply FOV = create perspective projection
    float a = Camera_fovX * ((fromPixel.x - halfWidth) / halfWidth);
    float b = Camera_fovY * ((halfHeight - fromPixel.y) / halfHeight);

    vec3 dir = normalize(a * Camera_right + b * Camera_up + Camera_direction);

    return Ray(Camera_position, dir); /** The ray begins in camera posiiton
                                            * and has the direction that corresponds to currently rendered pixel
                                            */
}

Hit findObjectHit(Ray ray)
{
    float closestDistance = POSITIVE_INFINITY;
    uint hitObjectIndex;
    vec3 hitPosition;
    vec3 normalAtHit;
    vec3 closesHitPosition = vec3(0, 0, 0);
    vec3 closestNormalAtHit = vec3(0, 0, 0);
    //Firstly try hitting objects
    for (int k = 0; k < objects.length(); ++k)
    {
        if (getRaySphereIntersection(objects[k], ray, hitPosition, normalAtHit)) // Update hit position and normal
        {
            float lastDistance = distance(Camera_position, hitPosition);
            if (lastDistance < closestDistance) {
                hitObjectIndex = k;
                closesHitPosition = hitPosition;
                closestNormalAtHit = normalAtHit;
                closestDistance = lastDistance; // Update closest distance
            }
        }
    }
    
    return Hit(closesHitPosition, closestNormalAtHit, hitObjectIndex);
}

vec3 computeObjectColor(Hit hit)
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
        bool inShadow = false;
        for (int k = 0; k < objects.length(); ++k) {
            if(k == hit.hitObjectIndex)
            {
                continue;
            }
            Ray shadowRay = Ray(hit.position, light.direction.xyz);
            float t0 = 0, t1 = 0;
            if (raySphereIntersection(objects[k].position, objects[k].radius, shadowRay, t0, t1)) {
                return AMBIENT_LIGHT;
            }
        }
        totalLightColor += light.color.xyz * dot(light.direction.xyz, hit.normalAtHit);
    }
    
    Material objMaterial = materials[objects[hit.hitObjectIndex].materialIndex];
    return (objMaterial.albedo.xyz * totalLightColor) + objMaterial.emission;
}

vec3 takeSample(vec2 fromPixel)
{
    // Create primary ray with direction for this pixel
    Ray primaryRay = createCameraRay(fromPixel);

    // Cast the ray into the scene and check for the intersection points
    Hit hit = findObjectHit(primaryRay);

    // Return color of the object at the hit
    if (hit.position != vec3(0,0,0)) // The zero vector indicates no hit
    {
        return computeObjectColor(hit);
    }
    else
    {
        //Then try hitting atmospheres
        for (int k = 0; k < atmospheres.length(); ++k)
        {
            float t0, t1;
            float tMax = POSITIVE_INFINITY;
            if(raySphereIntersection(atmospheres[k].center, atmospheres[k].startRadius, primaryRay, t0, t1) && t1 > 0)
            {
                // When the bottom of the atmosphere is intersecting primaryRay in positive direction
                // we need to limit the scattering computation to the inner atmosphere bounds (by tMax variable)
                tMax = max(t0, 0);
            }
            return atmosphereColor(atmospheres[k], primaryRay, 0, tMax);
        }

        return vec3(1,0,0);
    }
}
vec3 raytrace(vec2 fromPixel)
{
    vec3 resultColor = vec3(0, 0, 0);
    uint sampleNum = 0;
    for(int x = 0; x < Multisampling_perPixel; x++)
    {
        for(int y = 0; y < Multisampling_perPixel; y++,sampleNum++)
        {
            if(sampleNum == 0)
            {
                resultColor += takeSample(fromPixel + 0.5);
                continue;
            }
            vec2 randomOffset = vec2((x + random(fromPixel+x+y)) / Multisampling_perPixel, (y + random(fromPixel-x-y)) / Multisampling_perPixel);
            resultColor += takeSample(fromPixel + randomOffset);
        }
    }
    return resultColor / sampleNum;
}
