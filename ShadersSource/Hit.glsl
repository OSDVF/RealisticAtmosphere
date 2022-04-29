//?#version 440
#ifndef HIT_H
#define HIT_H
#include "Intersections.glsl"
#include "Buffers.glsl"
Hit findObjectHit(Ray ray, bool includeTranslucent)
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
        if(!includeTranslucent && materials[objects[k].materialIndex].albedo.a == 0)
            continue;
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

Hit findObjectHit(Ray ray)
{
    return findObjectHit(ray, true);
}
#endif