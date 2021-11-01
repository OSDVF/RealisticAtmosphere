//?#version 440
#include "Intersections.glsl"
#include "Random.glsl"

#ifdef DEBUG
    uniform vec4 debugAttributes;
    #define DEBUG_NORMALS (debugAttributes.x == 1.0)
#else
    #define DEBUG_NORMALS false
#endif

#define SPACE_COLOR vec3(0.1,0.1,0.3)
#define AMBIENT_LIGHT vec3(0)

layout(std430, binding=1) readonly buffer ObjectBuffer
{
    Sphere objects[];
};

layout(std430, binding=2) readonly buffer MaterialBuffer
{
    Material materials[];
};

layout(std430, binding=3) readonly buffer DirectionalLightBuffer
{
    DirectionalLight directionalLights[];
};

layout(std430, binding=4) readonly buffer PointLightBuffer
{
    PointLight pointLights[];
};

layout(std430, binding=5) readonly buffer SpotLightBuffer
{
    SpotLight spotLights[];
};

layout(std430, binding=6) readonly buffer AtmosphereBuffer
{
    Atmosphere atmosphere[];
};

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

Hit findClosestHit(Ray ray)
{
    float closestDistance = /*positive infinity*/ uintBitsToFloat(0x7F800000);
    uint hitObjectIndex;
    vec3 hitPosition;
    vec3 normalAtHit;
    vec3 closesHitPosition = vec3(0, 0, 0);
    vec3 closestNormalAtHit = vec3(0, 0, 0);
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

vec3 computeColor(Hit hit)
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
    Hit hit = findClosestHit(primaryRay);

    // Return color of the object at the hit
    if (hit.position != vec3(0,0,0)) // The zero vector indicates no hit
    {
        return computeColor(hit);
    }
    else
        return SPACE_COLOR;
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
