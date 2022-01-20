//?#version 440
#include "Intersections.glsl"
#include "Planet.glsl"
#include "Random.glsl"

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
    uint hitObjectIndex = -1;
    vec3 hitPosition;
    vec3 normalAtHit;
    vec3 closesHitPosition = vec3(0);
    vec3 closestNormalAtHit = vec3(0);
    //Firstly try hitting objects
    for (int k = 0; k < objects.length(); ++k)
    {
        float lastDistance;
        if (getRaySphereIntersection(objects[k], ray, hitPosition, normalAtHit, lastDistance)) // Update hit position and normal
        {
            if (lastDistance < closestDistance) {
                hitObjectIndex = k;
                closesHitPosition = hitPosition;
                closestNormalAtHit = normalAtHit;
                closestDistance = lastDistance; // Update closest distance
            }
        }
    }
    
    return Hit(closesHitPosition, closestNormalAtHit, hitObjectIndex, closestDistance);
}

vec3 takeSample(vec2 fromPixel)
{
    // Create primary ray with direction for this pixel
    Ray primaryRay = createCameraRay(fromPixel);

    // Cast the ray into the scene and check for the intersection points
    Hit hit = findObjectHit(primaryRay);
    vec3 objectColor = AMBIENT_LIGHT;
    // Return color of the object at the hit
    if (hit.hitObjectIndex != POSITIVE_INFINITY)
    {
        vec3 totalLightColor = computeLightColor(hit);
        Material objMaterial = materials[objects[hit.hitObjectIndex].materialIndex];
        objectColor = (objMaterial.albedo.xyz * totalLightColor) + objMaterial.emission;
    }
    
    //Then add the atmosphere color
    vec3 planetNatmoColor;
    float somePlanetHitT = planetsWithAtmospheres(primaryRay, hit.t, /*out*/ planetNatmoColor);
    if(somePlanetHitT < hit.t)// The planet surface is closer than the object
    {
        return planetNatmoColor;
    }
    else
    {
        return objectColor;
    } 
}

vec3 raytrace(vec2 fromPixel)
{
    fromPixel+= 0.5;//Center the ray
    vec3 resultColor = vec3(0, 0, 0);
    uint sampleNum = 0;
    switch(int(Multisampling_type))
    {
    case 1:
        for(int x = 0; x < Multisampling_perPixel; x++)
        {
            for(int y = 0; y < Multisampling_perPixel; y++,sampleNum++)
            {
                if(sampleNum == 0)
                {
                    resultColor += takeSample(fromPixel);
                    continue;
                }
                float firstRand = random(x);
                vec2 randomOffset = vec2(firstRand,random(firstRand))-0.5;
                vec2 griddedOffset = vec2(x / Multisampling_perPixel, y / Multisampling_perPixel) - 0.5;
                resultColor += takeSample(fromPixel + mix(griddedOffset,randomOffset,0.5));
            }
        }
        return resultColor / sampleNum;
    case 2:
        for(int x = 0; x < Multisampling_perPixel; x++)
        {
            for(int y = 0; y < Multisampling_perPixel; y++,sampleNum++)
            {
                vec2 griddedOffset = vec2(x / (Multisampling_perPixel-1), y / (Multisampling_perPixel-1)) - 0.5;
                resultColor += takeSample(fromPixel + griddedOffset);
            }
        }
        return resultColor / sampleNum;

    default:
        resultColor += takeSample(fromPixel);
        for(int x = 1; x < Multisampling_perPixel; x++)
        {
            float firstRand = random(x);
            vec2 randomOffset = vec2(firstRand,random(firstRand))-0.5;
            resultColor += takeSample(fromPixel + randomOffset);
        }
        return resultColor / Multisampling_perPixel;
    }
}
