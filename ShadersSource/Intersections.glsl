//?#version 440
#ifndef INTERSECTIONS
#define INTERSECTIONS
struct Ray
{
    vec3 origin;
    vec3 direction;
};

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

bool rayBoxIntersection(vec3 position, vec3 size, Ray ray, out float tmin, out float tmax)
{
    // https://tavianator.com/2011/ray_box.html
    vec3 minPos = position;
    vec3 maxPos = position + size;

    vec3 t1 = (minPos - ray.origin)/ray.direction;
    vec3 t2 = (maxPos - ray.origin)/ray.direction;

    tmin = max(
            min(t1.x, t2.x), 
            max(
                min(t1.y, t2.y), min(t1.z, t2.z)
            )
        );
    tmax = min(
            max(t1.x, t2.x),
            min(
                max(t1.y, t2.y), max(t1.z, t2.z)
            )
        );

    if(tmax < 0 || tmin > tmax)
        return false;

    return true;
}

// With normal
bool getRayBoxIntersection(AnalyticalObject box, Ray ray, out vec3 hitPosition, out vec3 normalAtHit, out float t0)
{
    vec3 minPos = box.position;
    vec3 maxPos = box.position + box.size;

    vec3 t1 = (minPos - ray.origin)/ray.direction;
    vec3 t2 = (maxPos - ray.origin)/ray.direction;

    vec3 tmin = 
        vec3(
            min(t1.x, t2.x),
            min(t1.y, t2.y),
            min(t1.z, t2.z)
        );
    vec3 tmax =
        vec3(
            max(t1.x, t2.x),
            max(t1.y, t2.y),
            max(t1.z, t2.z)
        );

    float allMax = min(min(tmax.x,tmax.y),tmax.z);
    float allMin = max(max(tmin.x,tmin.y),tmin.z);

    if(allMax > 0.0 && allMax > allMin)
    {
        hitPosition = ray.origin + ray.direction * allMin;
        vec3 center = box.position + box.size*0.5;
        vec3 difference = center - hitPosition;
        if (allMin == tmin.x) {
            normalAtHit = vec3(1, 0, 0) * sign(difference.x);
        }
        else if (allMin == tmin.y) {
            normalAtHit = vec3(0, 1, 0) * sign(difference.y);
        }
        else if (allMin == tmin.z) {
            normalAtHit = vec3(0, 0, 1) * sign(difference.z);
        } 
        t0 = allMin;
        return true;
    }
    return false;
}


//Geometric solution, with normal
bool getRaySphereIntersection(AnalyticalObject sphere, Ray ray, out vec3 hitPosition, out vec3 normalAtHit, out float t0)
{
    hitPosition = vec3(0,0,0);
    normalAtHit = vec3(0,0,0);
    vec3 L = sphere.position - ray.origin;
    float tca = dot(ray.direction, L);
    if(tca < 0)
        return false;
    float radius2 = sphere.size.x * sphere.size.x;
    float d2 = dot(L,L) - tca * tca; 
    if (d2 > radius2) return false; 

    float thc = sqrt(radius2 - d2); 
    t0 = tca - thc; 
    float t1 = tca + thc; 

    vec3 hp = ray.origin + ray.direction * t0; // point of intersection 
    hitPosition = hp;
    normalAtHit = normalize(hp - sphere.position);
    return true; 
}


bool rayObjectIntersection(AnalyticalObject object, Ray ray, out float t0, out float t1)
{
    switch(object.type)
    {
    case 0:
        //Sphere
        return raySphereIntersection(object.position, object.size.x, ray, t0, t1);
    case 1:
        return rayBoxIntersection(object.position, object.size, ray, t0, t1);
    }
}

bool getRayObjectIntersection(AnalyticalObject object, Ray ray, out vec3 hitPosition, out vec3 normalAtHit, out float t0)
{
    switch(object.type)
    {
    case 0:
        //Sphere
        return getRaySphereIntersection(object, ray, hitPosition, normalAtHit, t0);
    case 1:
        return getRayBoxIntersection(object, ray, hitPosition, normalAtHit, t0);
    }
}
#endif