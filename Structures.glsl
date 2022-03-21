//?#version 440
#ifndef STRUCTURES_H
#define STRUCTURES_H
const int SCATTERING_TEXTURE_R_SIZE = 32;
const int SCATTERING_TEXTURE_MU_SIZE = 128;
const int SCATTERING_TEXTURE_MU_S_SIZE = 32;
const int SCATTERING_TEXTURE_NU_SIZE = 8;
const float SCATTERING_TEXTURE_LIGHT_COUNT = 2;

const int SCATTERING_TEXTURE_HEIGHT = SCATTERING_TEXTURE_MU_SIZE * int(SCATTERING_TEXTURE_LIGHT_COUNT);

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

    vec3 irradiance;
    float intensity;
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

    vec3 absorptionCoefficients; // Ozone layer
    float mountainsRadius;
    
    float atmosphereThickness;// Should be precomputed as (atmosphereRadius - surfaceRadius)
    float ozonePeakHeight;
    float ozoneTroposphereCoef;
    float ozoneTroposphereConst;

    float ozoneStratosphereCoef;
    float ozoneStratosphereConst;
    uint firstLight;
    uint lastLight;
    
    CloudLayer clouds;

    float firstLightCoord;
    float padding1;
    float padding2;
    float padding3;
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