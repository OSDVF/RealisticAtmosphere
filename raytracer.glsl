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

// https://www.shadertoy.com/view/4lfcDr
vec2
sample_disk(vec2 uv)
{
	float theta = 2.0 * 3.141592653589 * uv.x;
	float r = sqrt(uv.y);
	return vec2(cos(theta), sin(theta)) * r;
}

vec3
sample_cos_hemisphere(vec2 uv)
{
	vec2 disk = sample_disk(uv);
	return vec3(disk.x, sqrt(max(0.0, 1.0 - dot(disk, disk))), disk.y);
}

mat3
construct_ONB_frisvad(vec3 normal)
{
	mat3 ret;
	ret[1] = normal;
	if(normal.z < -0.999805696) {
		ret[0] = vec3(0.0, -1.0, 0.0);
		ret[2] = vec3(-1.0, 0.0, 0.0);
	}
	else {
		float a = 1.0 / (1.0 + normal.z);
		float b = -normal.x * normal.y * a;
		ret[0] = vec3(1.0 - normal.x * normal.x * a, b, -normal.x);
		ret[2] = vec3(b, 1.0 - normal.y * normal.y * a, -normal.y);
	}
	return ret;
}

Ray createSecondaryRay(Hit fromHit)
{
    float seed = time.x*fromHit.position.x;
    vec2 randomValues = vec2(random(seed),random(seed+1));
    mat3 onb = construct_ONB_frisvad(fromHit.normalAtHit);
    vec3 dir = normalize(onb * sample_cos_hemisphere(randomValues));
    Ray ray_next = Ray(fromHit.position, dir);
	ray_next.origin += ray_next.direction * 1e-5;
    return ray_next;
}

vec3 takeSample(vec2 fromPixel)
{
    // Create primary ray with direction for this pixel
    Ray primaryRay = createCameraRay(fromPixel);
    // Cast the ray into the scene and check for the intersection points
    Hit objectHit = findObjectHit(primaryRay);
    vec3 radiance = vec3(0);
    vec3 throughput = vec3(1);
    // Return color of the object at the hit
    
    // Then add the atmosphere color
    Hit planetOrObjHit;
    bool somethingHit = planetsWithAtmospheres(primaryRay, objectHit.t, /*inout*/ radiance, /*inout*/ throughput, /*out*/ planetOrObjHit);
    if (!somethingHit && objectHit.hitObjectIndex != -1)
    {
        Material objMaterial = materials[objects[objectHit.hitObjectIndex].materialIndex];
        vec3 totalLightColor = computeLightColor(objectHit);
        radiance = (totalLightColor * objMaterial.albedo.xyz + objMaterial.emission);
        throughput = objMaterial.albedo.xyz;
    }
    //
    // Bounces
    //
    int bounces = floatBitsToInt(Multisampling_maxBounces);
    if(bounces > 0)
    {
        int b;
        // Create secondary rays
        for(b = 0; b < bounces; b++)
        {
            if(objectHit.hitObjectIndex != -1)
            {
                somethingHit = true;
                planetOrObjHit = objectHit;
            }
            if(somethingHit)
            {
                Ray secondaryRay = createSecondaryRay(planetOrObjHit);

                objectHit = findObjectHit(secondaryRay);
                somethingHit = planetsWithAtmospheres(secondaryRay, objectHit.t, /*inout*/ radiance, /*inout*/ throughput, /*out*/ planetOrObjHit);
                if (!somethingHit && objectHit.hitObjectIndex != -1)
                {
                    Material objMaterial = materials[objects[objectHit.hitObjectIndex].materialIndex];
                    vec3 totalLightColor = computeLightColor(objectHit);
                    radiance += (objMaterial.emission + totalLightColor * objMaterial.albedo.xyz) * throughput;
                    throughput *= objMaterial.albedo.xyz;
                }
            }
            else
            {
                break;
            }
        }
    }
    return radiance;
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
