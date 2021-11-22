//?#version 440
#ifndef INTERSECTIONS
#define INTERSECTIONS
#include "SceneObjects.glsl"
bool solveQuadratic(float a, float b, float c, out float x1, out float x2) 
{ 
    if (b == 0) { 
        // Handle special case where the the two vector ray.dir and V are perpendicular
        // with V = ray.orig - sphere.centre
        if (a == 0) return false; 

        x1 = 0;
        x2 = sqrt(-c / a); 
        return true; 
    } 
    float discr = b * b - 4 * a * c; 
 
    if (discr < 0) return false; 
 
    float q = (b < 0.f) ? -0.5f * (b - sqrt(discr)) : -0.5f * (b + sqrt(discr)); 
    x1 = q / a; 
    x2 = c / q; 
 
    return true; 
} 
//Analytic solution
bool raySphereIntersection(vec3 position, float radius, Ray ray, out float t0, out float t1)
{
 // They ray dir is normalized so A = 1 
    vec3 L = ray.origin - position;
    float A = dot(ray.direction, ray.direction); 
    float B = 2 * dot(ray.direction, L); 
    float C = dot(L, L) - radius * radius; 
 
    if (!solveQuadratic(A, B, C, t0, t1)) return false;  
    if (t0 > t1)
    {
        float swap = t0;
        t0 = t1;
        t1 = swap;
    }
    return true; 
}


//Geometric solution
bool getRaySphereIntersection(Sphere sphere, Ray ray, out vec3 hitPosition, out vec3 normalAtHit)
{
    hitPosition = vec3(0,0,0);
    normalAtHit = vec3(0,0,0);
    vec3 L = sphere.position - ray.origin;
    float tca = dot(ray.direction, L);
    if(tca < 0)
        return false;
    float radius2 = sphere.radius * sphere.radius;
    float d2 = dot(L,L) - tca * tca; 
    if (d2 > radius2) return false; 

    float thc = sqrt(radius2 - d2); 
    float t0 = tca - thc; 
    float t1 = tca + thc; 

    vec3 hp = ray.origin + ray.direction * t0; // point of intersection 
    hitPosition = hp;
    normalAtHit = normalize(hp - sphere.position);
    return true; 
}
#endif