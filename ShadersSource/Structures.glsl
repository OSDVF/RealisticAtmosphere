//?#version 440
// Will be included in .cpp and also .glsl
#ifndef STRUCTURES_H
#define STRUCTURES_H

// Definition of common constants for C++ and GLSL code
#define SCATTERING_LIGHT_COUNT 1

#if SCATTERING_LIGHT_COUNT > 1
#define IRRADIANCE_COORD_TYPE vec3
#define IRRADIANCE_SAMPLER_TYPE sampler2DArray
#else
#define IRRADIANCE_COORD_TYPE vec2
#define IRRADIANCE_SAMPLER_TYPE sampler2D
#endif


const int SCATTERING_TEXTURE_R_SIZE = 32;
const int SCATTERING_TEXTURE_MU_SIZE = 128;
const int SCATTERING_TEXTURE_MU_S_SIZE = 32;
const int SCATTERING_TEXTURE_NU_SIZE = 8;

const int SCATTERING_TEXTURE_HEIGHT = SCATTERING_TEXTURE_MU_SIZE * SCATTERING_LIGHT_COUNT;

struct CloudLayer
{
    vec3 position;
    float coverage;

    float startRadius;
    float endRadius;
    float layerThickness;// Should be precomputed as (cloudsEndRadius - cloudsStartRadius)
    float density;

    float lowerGradient;
    float upperGradient;
    float scatteringCoef;
    float extinctionCoef;

    vec3 sizeMultiplier;
    float sharpness;
};

struct DirectionalLight
{
    vec3 direction;
    float angularRadius;

    vec3 irradiance;//Irradiance at atmosphere boundary
    float padding;
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

struct AnalyticalObject
{
    vec3 position;
    uint type; //0 = sphere, 1 = cube
    vec3 size;
    uint materialIndex;
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

    vec3 absorptionCoefficients; // Ozone layer
    float mountainsRadius;

    float mountainsHeight;
    float atmosphereThickness;// Should be precomputed as (atmosphereRadius - surfaceRadius)
    float ozonePeakHeight;
    float ozoneTroposphereCoef;

    float ozoneTroposphereConst;
    float ozoneStratosphereCoef;
    float ozoneStratosphereConst;
    float firstLightCoord;

    uint firstLight;
    uint lastLight;
    float mu_s_min;
    float padding;

    CloudLayer clouds;

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