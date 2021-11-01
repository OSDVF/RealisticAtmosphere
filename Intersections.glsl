//?#version 440
#include "SceneObjects.glsl"
bool raySphereIntersection(vec3 position, float radius, Ray ray, out float t0, out float t1)
{
    vec3 distanceOriginToCenter = position - ray.origin;
    float distanceOriginToPerpendicular = dot(ray.direction, distanceOriginToCenter);
    if(distanceOriginToPerpendicular < 0)
        return false;
    float distanceCenterToSecular2 = dot(distanceOriginToCenter, distanceOriginToCenter) - distanceOriginToPerpendicular * distanceOriginToPerpendicular; 
    float radius2 = radius * radius;
    if (distanceCenterToSecular2 > radius2)
        return false; 
    
    float distanceIntersectionToSecular = sqrt(radius2 - distanceCenterToSecular2); 
    t0 = distanceOriginToPerpendicular - distanceIntersectionToSecular; 
    t1 = distanceOriginToPerpendicular + distanceIntersectionToSecular;
    if (t0 > t1)
    {
        float swap = t0;
        t0 = t1;
        t1 = swap;
    }

    return true; 
}

bool getRaySphereIntersection(Sphere sphere, Ray ray, out vec3 hitPosition, out vec3 normalAtHit)
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