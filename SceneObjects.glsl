//?#version 430
//!#define BGFX_SHADER_LANGUAGE_GLSL
// FOR SOME REASON ALL UNIFORMS MUST BE DEFINED FIRST TO BE RECOGNIZED BY BGFX ANNOYING SHADERC

/**
 * Represents camera matrix (location, direction, up, right, fovX, fovY)
 */
#ifdef BGFX_SHADER_LANGUAGE_GLSL
uniform vec4 u_viewRect;
uniform vec4 Camera[4];
#else
vec4 Camera[] =
{
    vec4(0,0,0,1),
    vec4(0,0,1,0),
    vec4(0,1,0,0),
    vec4(1,0,0,0)
}
#endif
;

struct Ray
{
    vec3 origin;
    vec3 direction;
};
#define Camera_position (Camera[0].xyz)
#define Camera_direction (Camera[1].xyz)
#define Camera_up (Camera[2].xyz)
#define Camera_right (Camera[3].xyz)
#define Camera_fovX (Camera[3].w)
#define Camera_fovY (Camera[2].w)

struct DirectionalLight
{
    vec4 direction;
    vec4 color;
};

struct SpotLight
{
    vec3 position;
    vec3 direction;
    vec3 color;
    vec2 radius;
};

struct PointLight
{
    vec3 position;
    vec3 color;
    vec3 attenuation;
};

struct Sphere
{
    vec3 position;
    float radius;
    uint materialIndex;
    float _pad1;/*std430 memory padding to multiplies of vec4 */
    float _pad2;
    float _pad3;
};

struct Hit
{
    vec3 position;
    vec3 normalAtHit;
    uint hitObjectIndex;
};

/**
 * Albedo, Smoothness, Metalness, Emission
 */
struct Material {
    vec4 albedo;
    vec3 specular;
    float smoothness;
    vec3 emission;
    float occlusion;
};
