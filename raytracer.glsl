//?#version 440
#include "Intersections.glsl"
#include "Atmosphere.glsl"
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
        //Then try hitting the planets
        for (int k = 0; k < planets.length(); ++k)
        {
            float tMax;
            vec3 planet = raytracePlanet(planets[k], primaryRay, tMax);//tMax will be ray direciton multiplier if the ray hits the planet
            return planet + atmosphereColor(planets[k], primaryRay, 0, tMax);
        }

        return AMBIENT_LIGHT;
    }
}

vec3 tonemapping(vec3 hdrColor)
{
    return min(min(hdrColor.x,hdrColor.y),hdrColor.z) < 1.413 ? /*gamma correction*/ pow(hdrColor * 0.38317, vec3(1.0 / 2.2)) : 1.0 - exp(-hdrColor)/*exposure tone mapping*/;
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
        return tonemapping(resultColor / sampleNum);
    case 2:
        for(int x = 0; x < Multisampling_perPixel; x++)
        {
            for(int y = 0; y < Multisampling_perPixel; y++,sampleNum++)
            {
                vec2 griddedOffset = vec2(x / (Multisampling_perPixel-1), y / (Multisampling_perPixel-1)) - 0.5;
                resultColor += takeSample(fromPixel + griddedOffset);
            }
        }
        return tonemapping(resultColor / sampleNum);

    default:
        resultColor += takeSample(fromPixel);
        for(int x = 1; x < Multisampling_perPixel; x++)
        {
            float firstRand = random(x);
            vec2 randomOffset = vec2(firstRand,random(firstRand))-0.5;
            resultColor += takeSample(fromPixel + randomOffset);
        }
        return tonemapping(resultColor / Multisampling_perPixel);
    }
}
