//?#version 440
#include "Hit.glsl"
#include "Planet.glsl"
#include "Random.glsl"

float deg2rad(float deg){
    return deg*pi / 180.0;
}

// Create a primary ray for pixel x, y
Ray createCameraRay(vec2 fromPixel)
{
    float halfWidth = u_viewRect.z / 2.0f;
    float halfHeight = u_viewRect.w / 2.0f;

    float ndcX = ((fromPixel.x - halfWidth) / halfWidth);
    float ndcY = ((halfHeight - fromPixel.y) / halfHeight);
    if(Camera_fovY == 0)
    {
        vec3 dir;
        if(Camera_fovX == 0)
        {
            // Create spherical "fisheye" cam
            float zSquared = ndcX * ndcX + ndcY * ndcY;
            float phi = atan(ndcY, ndcX);
            float theta = acos(1 - zSquared);

            dir = normalize(
                    sin(theta) * cos(phi) * Camera_right 
                    - cos(theta) * Camera_up 
                    + sin(theta) * sin(phi)*Camera_direction
                );
        
        }
        else
        {
             // Create equirectangular projection
            float fovX = 2.*pi;
            float fovY = pi;
            float hOffset = (2.0*pi - fovX)*0.5;
            float vOffset = (pi - fovY)*0.5;

            vec2 interp = vec2(ndcX,ndcY)*0.5 + 0.5;
            float hAngle = hOffset + interp.x * fovX;
            float vAngle = vOffset + interp.y * fovY;
            dir = vec3( 
                    sin(vAngle) * sin(hAngle),
                    -cos(vAngle),
                    sin(vAngle) * cos(hAngle)
                );
        }
    
        return Ray(Camera_position, dir);
    }
    else
    {
        // Apply FOV = create perspective projection
        float a = Camera_fovX * ndcX;
        float b = Camera_fovY * ndcY;

        vec3 dir = normalize(a * Camera_right + b * Camera_up + Camera_direction);

        return Ray(Camera_position, dir); /** The ray begins in camera posiiton
                                                * and has the direction that corresponds to currently rendered pixel
                                                */
    }
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

Ray createSecondaryRay(vec2 coord, vec3 pos, vec3 normal)
{
    float seed = time.x*pos.x*pos.y*pos.z*coord.x;
    vec2 randomValues = vec2(random(seed),random(seed*coord.y));
    mat3 onb = construct_ONB_frisvad(normal);
    vec3 dir = normalize(onb * sample_cos_hemisphere(randomValues));
    Ray ray_next = Ray(pos, dir);
	ray_next.origin += ray_next.direction * LightSettings_secondaryOffset;//Offset to prevent self-blocking
    return ray_next;
}

vec2 getSubpixelCoords(vec2 fromPixel, float directSampleNum)
{
    fromPixel+= 0.5;//Center the ray
    float multisampling = HQSettings_directSamples;
    float x,y;
    switch(floatBitsToInt(Multisampling_type))
    {
    case 1:
        x = mod(directSampleNum, multisampling);
        y = directSampleNum / multisampling;
        if(directSampleNum == 0)
        {
            return fromPixel;
        }
        else
        {
            float firstRand = random(directSampleNum);
            vec2 randomOffset = vec2(firstRand,random(firstRand))-0.5;
            vec2 griddedOffset = vec2(x / multisampling, y / multisampling) - 0.5;
            return fromPixel + mix(griddedOffset, randomOffset, 0.5);
        }
    case 2:
        {
            x = mod(directSampleNum, multisampling);
            y = directSampleNum / multisampling;
            vec2 griddedOffset = vec2(x / (multisampling-1), y / (multisampling-1)) - 0.5;
            return fromPixel + griddedOffset;
        }

    default:
        if(directSampleNum == 0)
        {
            return fromPixel;
        }
        else
        {
            float firstRand = random(directSampleNum);
            vec2 randomOffset = vec2(firstRand,random(firstRand))-0.5;
            return fromPixel + randomOffset;
        }
    }
}

void raytraceSecondary(vec2 subpixelCoord, vec3 origin, vec3 normal, vec3 throughput, float invIndirectCount, inout vec3 colorOut)
{
    // Switch to lower quality for secondary rays:
    HQSettings_atmoCompute = false;
    HQSettings_lightShafts = false;

    // Traces secondary ray from the point specified by "normal, albedo, depth = primary ray length" parameters
    int bounces = floatBitsToInt(Multisampling_maxBounces);
    // Create secondary rays
    for(int b = 0; b < bounces; b++)
    {
        Ray secondaryRay = createSecondaryRay(subpixelCoord, origin, normal);

        Hit objectHit = findObjectHit(secondaryRay);
        Hit planetHit;
        vec3 atmColor = vec3(0);
        bool terrainWasHit;
        if(HQSettings_indirectApprox && objectHit.hitObjectIndex == -1)
        {
            //Indirect lighting from atmosphere is already computed
            //So compute only lighting reflected from terrain
            terrainWasHit = terrainColorAndHit(planets[0], secondaryRay, 0, objectHit.t, throughput, atmColor, planetHit);
        }
        else
        {
            terrainWasHit = planetsWithAtmospheres(secondaryRay, objectHit.t, /*out*/ atmColor, /*inout*/ throughput, /*out*/ planetHit);
        }
        // atmColor has throughput applied
        colorOut += atmColor * invIndirectCount;
        if(terrainWasHit)
        {
            origin = planetHit.position;
            normal = planetHit.normalAtHit;
        }
        if (objectHit.hitObjectIndex != -1 && objectHit.t <= planetHit.t)
        {
            Material objMaterial = materials[objects[objectHit.hitObjectIndex].materialIndex];
            vec3 totalLightColor = objectIlluminance(objectHit);
            colorOut += (objMaterial.emission.rgb + totalLightColor * objMaterial.albedo.rgb)  * objMaterial.albedo.a * throughput * invIndirectCount;
            throughput *= objMaterial.albedo.rgb * objMaterial.albedo.a;

            origin = objectHit.position;
            normal = objectHit.normalAtHit;
        }
        else
        {
            // No hit -> add atmosphere color end
            return;
        }
    }
}

vec3 raytracePrimSec(vec2 subpixelCoord, float invIndirectCount, out vec3 normal, out vec3 throughput, out float depth)
{
    // Traces both primary and secondary ray
    vec3 colorOut = vec3(0);
    normal = vec3(0);
    throughput = vec3(1);// save initial throughput to albedo
    depth = 0;//No hit

    Ray primaryRay = createCameraRay(subpixelCoord);

    // Cast the ray into the scene and check for the intersection points with analytical objects
    Hit objectHit = findObjectHit(primaryRay);
    Hit planetHit;
    // Add the atmosphere and planet color
    vec3 atmColor = vec3(0);
    bool terrainWasHit = planetsWithAtmospheres(primaryRay, objectHit.t, /*out*/ atmColor, /*inout*/ throughput, /*out*/ planetHit);
    colorOut += atmColor;
    if(terrainWasHit)
    {
        // Hit a terrain on planet
        normal = planetHit.normalAtHit;
        depth = planetHit.t;

        vec3 throughputForSecondary = throughput;//We don't want to capture the secondary ray's throughput into albedo buffer
        raytraceSecondary(subpixelCoord, planetHit.position, normal, throughputForSecondary, invIndirectCount, colorOut);
    }
    if (objectHit.hitObjectIndex != -1 && objectHit.t <= planetHit.t)
    {
        // Also hit an analytical object
        // Add color of the object at the hit
        Material objMaterial = materials[objects[objectHit.hitObjectIndex].materialIndex];
        vec3 totalLightColor = objectIlluminance(objectHit);
        
        colorOut += (objMaterial.albedo.rgb * totalLightColor + objMaterial.emission.rgb) * throughput;
        throughput *= objMaterial.albedo.rgb;//Transparency is ignored here as it is not really implemented and only used to identify 'ghost' objects (e.g. sun)
        normal = objectHit.normalAtHit;
        if(objMaterial.albedo.a != 0) // Do not put transparent objects into depth buffer
            depth = objectHit.t;

        vec3 throughputForSecondary = throughput;
        raytraceSecondary(subpixelCoord, objectHit.position, normal, throughputForSecondary, invIndirectCount, colorOut);
    }
    else
    {
        // Hit only atmosphere or spherical planet surface
        if(planetHit.t == POSITIVE_INFINITY)
        {
            depth = 0;
        }
        else
        {
            depth = planetHit.t;
        }

    }
    return colorOut;
}
