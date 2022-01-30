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
const float POSITIVE_INFINITY = 3.402823466e+38;
const float NEGATIVE_INFINITY = uintBitsToFloat(0xFF800000);
#define SPACE_COLOR vec3(0.1,0.1,0.3)
uniform sampler2D texSampler1;
uniform sampler2D texSampler2;
uniform sampler2D texSampler3;
uniform sampler2D heightmapTexture;
uniform vec4 u_viewRect;
uniform vec4 Camera[4];
uniform vec4 MultisamplingSettings;
uniform vec4 QualitySettings;
uniform vec4 PlanetMaterial;
uniform vec4 RaymarchingSteps;
uniform vec4 HQSettings;
uniform vec4 LightSettings;
uniform vec4 LightSettings2;
#else
vec4 Camera[] =
{
    vec4(0,0,0,1),//Position
    vec4(0,0,1,0),//Direction
    vec4(0,1,0,0),//Up vector, fovY
    vec4(1,0,0,0)//Right vector, fovX
};
int perPixel = 1;
int bounces = 0;
int perAtmosphere = 64;
int type = 0;
vec4 MultisamplingSettings = {*(float*)&perPixel,*(float*)&bounces,*(float*)&perAtmosphere,*(float*)&type};
vec4 QualitySettings = {5,50,70000,1};
uint PathTracing = 1;
uint TerrainShadows = 2;
uint HQFlags1 = 0;
int currentSample = 0;
vec4 HQSettings = {*(float*)&HQFlags1, *(float*)&currentSample};
vec4 LightSettings = {1000, 0.03, 0.4, 0.02};
int lightTerrainDetectSteps = 40;
vec4 LightSettings2 = {0.5, *(float*)&lightTerrainDetectSteps, 3, 0.8};
vec4 PlanetMaterial = {1700, 2300, 1, 600};
int planetSteps = 200;
vec4 RaymarchingSteps = {*(float*)&planetSteps, 4, 0.005, 0.4};
#endif

#define Multisampling_perPixel MultisamplingSettings.x
#define Multisampling_maxBounces MultisamplingSettings.y
#define Multisampling_perAtmospherePixel MultisamplingSettings.z
#define Multisampling_type MultisamplingSettings.w

#define QualitySettings_steps QualitySettings.x
#define QualitySettings_minStepSize QualitySettings.y
#define QualitySettings_farPlane QualitySettings.z
#define QualitySettings_optimism QualitySettings.w

#define LightSettings_farPlane LightSettings.x
#define LightSettings_precision LightSettings.y
#define LightSettings_noRayThres LightSettings.z
#define LightSettings_viewThres LightSettings.w
#define LightSettings_cutoffDist LightSettings2.x
#define LightSettings_shadowSteps LightSettings2.y
#define LightSettings_gradient LightSettings2.z
#define LightSettings_fieldThres LightSettings2.z
#define LightSettings_terrainOptimMult LightSettings2.w

#define HQSettings_sampleNum HQSettings.y

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

struct Planet
{
    vec3 center;
    float surfaceRadius;
    float atmosphereRadius;
    float mieCoefficient;
    float mieAsymmetryFactor;
    float mieScaleHeight; /* Aerosol density would be uniform if the atmosfere was homogenous and had this "Scale" height */
    vec3 rayleighCoefficients;
    float rayleighScaleHeight; /* Air molecular density would be uniform if the atmosfere was homogenous and had this "Scale" height */
    float sunIntensity;
    uint sunDrectionalLightIndex;
    float mountainsRadius;
    float _pad2;
};

struct Hit
{
    vec3 position;
    vec3 normalAtHit;
    uint hitObjectIndex;
    float t;/**< Multiplier of ray direction */
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