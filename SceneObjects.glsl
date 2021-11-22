//?#version 430
//!#define BGFX_SHADER_LANGUAGE_GLSL
// FOR SOME REASON ALL UNIFORMS MUST BE DEFINED FIRST TO BE RECOGNIZED BY BGFX ANNOYING SHADERC

/**
 * Represents camera matrix (location, direction, up, right, fovX, fovY)
 */
#ifndef SCENEOBJECTS
#define SCENEOBJECTS
#define AMBIENT_LIGHT vec3(0)
#ifdef BGFX_SHADER_LANGUAGE_GLSL
const float POSITIVE_INFINITY = uintBitsToFloat(0x7F800000);
#define SPACE_COLOR vec3(0.1,0.1,0.3)
uniform vec4 u_viewRect;
uniform vec4 Camera[4];
uniform vec4 MultisamplingSettings;
#else
vec4 Camera[] =
{
    vec4(0,0,0,1),//Position
    vec4(0,0,1,0),//Direction
    vec4(0,1,0,0),//Up vector, fovY
    vec4(1,0,0,0)//Right vector, fovX
};
vec4 MultisamplingSettings = {2,8,16,0};
#endif

struct Ray
{
    vec3 origin;
    vec3 direction;
};
#define Multisampling_perPixel MultisamplingSettings.x
#define Multisampling_perLightRay MultisamplingSettings.y
#define Multisampling_perAtmospherePixel MultisamplingSettings.z

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

struct Atmosphere
{
    vec3 center;
    float startRadius;
    float endRadius;
    float mieCoefficient;
    float mieAsymmetryFactor;
    float mieScaleHeight; /* Aerosol density would be uniform if the atmosfere was homogenous and had this "Scale" height */
    vec3 rayleighCoefficients;
    float rayleighScaleHeight; /* Air molecular density would be uniform if the atmosfere was homogenous and had this "Scale" height */
    float sunIntensity;
    uint sunObjectIndex;
    float _pad1;
    float _pad2;
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
#endif