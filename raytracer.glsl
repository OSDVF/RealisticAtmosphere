//?#version 440
#include "SceneObjects.glsl"
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
bool isIntersecting(Sphere sphere, Ray ray)
{
    vec3 distanceOriginToCenter = sphere.position - ray.origin;
    float distanceOriginToPerpendicular = dot(ray.direction, distanceOriginToCenter);
    if(distanceOriginToPerpendicular < 0)
        return false;
    float distanceCenterToSecular2 = dot(distanceOriginToCenter, distanceOriginToCenter) - distanceOriginToPerpendicular * distanceOriginToPerpendicular; 
    float radius2 = sphere.radius * sphere.radius;
    if (distanceCenterToSecular2 > radius2)
        return false; 
    return true; 
}

bool getIntersection(Sphere sphere, Ray ray, out vec3 hitPosition, out vec3 normalAtHit)
{
    hitPosition = vec3(0,0,0);
    normalAtHit = vec3(0,0,0);
    vec3 distanceOriginToCenter = sphere.position - ray.origin;
    float distanceOriginToPerpendicular = dot(ray.direction, distanceOriginToCenter);
    if(distanceOriginToPerpendicular < 0)
        return false;
    float distanceCenterToSecular2 = dot(distanceOriginToCenter, distanceOriginToCenter) - distanceOriginToPerpendicular * distanceOriginToPerpendicular; 
    float radius2 = sphere.radius * sphere.radius;
    if (distanceCenterToSecular2 > radius2)
        return false; 
    float distanceIntersectionToSecular = sqrt(radius2 - distanceCenterToSecular2); 
    float t0 = distanceOriginToPerpendicular - distanceIntersectionToSecular; 
    float t1 = distanceOriginToPerpendicular + distanceIntersectionToSecular;
    if (t0 < 0) t0 = t1; 

    vec3 hp = ray.origin + ray.direction * t0; // point of intersection 
    hitPosition = hp;
    normalAtHit = normalize(hp - sphere.position);
    return true; 
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
        if (getIntersection(objects[k], ray, hitPosition, normalAtHit)) // Update hit position and normal
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
            if (isIntersecting(objects[k], shadowRay)) {
                return AMBIENT_LIGHT;
            }
        }
        totalLightColor += light.color.xyz * dot(light.direction.xyz, hit.normalAtHit);
    }
    
    Material objMaterial = materials[objects[hit.hitObjectIndex].materialIndex];
    return (objMaterial.albedo.xyz * totalLightColor) + objMaterial.emission;
}

vec3 raytrace(vec2 fromPixel)
{
    vec3 resultColor;
    // Create primary ray with direction for this pixel
    Ray primaryRay = createCameraRay(fromPixel);

    // Cast the ray into the scene and check for the intersection points
    Hit hit = findClosestHit(primaryRay);

    // Return color of the object at the hit
    if (hit.position != vec3(0,0,0)) // The zero vector indicates no hit
    {
        resultColor = computeColor(hit);
    }
    else
        resultColor = SPACE_COLOR;
    return resultColor;
}