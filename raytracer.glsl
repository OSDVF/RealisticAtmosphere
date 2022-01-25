//?#version 440
#include "Hit.glsl"
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

vec3 takeSample(vec2 fromPixel)
{
    // Create primary ray with direction for this pixel
    Ray primaryRay = createCameraRay(fromPixel);

    // Cast the ray into the scene and check for the intersection points
    Hit hit = findObjectHit(primaryRay);
    vec3 resultColor = vec3(0);
    // Return color of the object at the hit
    if (hit.hitObjectIndex != -1)
    {
        vec3 totalLightColor = computeLightColor(hit);
        Material objMaterial = materials[objects[hit.hitObjectIndex].materialIndex];
        resultColor = (objMaterial.albedo.xyz * totalLightColor) + objMaterial.emission;
    }
    
    //Then add the atmosphere color
    planetsWithAtmospheres(primaryRay, hit.t, /*inout*/ resultColor);
    return resultColor;
}

vec3 raytrace(vec2 fromPixel)
{
    fromPixel+= 0.5;//Center the ray
    int sampleNum = floatBitsToInt(HQSettings_sampleNum);
    int multisampling = floatBitsToInt(Multisampling_perPixel);
    int x,y;
    switch(floatBitsToInt(Multisampling_type))
    {
    case 1:
        x = sampleNum % multisampling;
        y = sampleNum / multisampling;
        if(sampleNum == 0)
        {
            return takeSample(fromPixel);
        }
        else
        {
            float firstRand = random(sampleNum);
            vec2 randomOffset = vec2(firstRand,random(firstRand))-0.5;
            vec2 griddedOffset = vec2(x / multisampling, y / multisampling) - 0.5;
            return takeSample(fromPixel + mix(griddedOffset, randomOffset, 0.5));
        }
    case 2:
        {
            x = sampleNum % multisampling;
            y = sampleNum / multisampling;
            vec2 griddedOffset = vec2(x / (multisampling-1), y / (multisampling-1)) - 0.5;
            return takeSample(fromPixel + griddedOffset);
        }

    default:
        if(sampleNum == 0)
        {
            return takeSample(fromPixel);
        }
        else
        {
            float firstRand = random(sampleNum);
            vec2 randomOffset = vec2(firstRand,random(firstRand))-0.5;
            return takeSample(fromPixel + randomOffset);
        }
    }
}
