//?#version 440
#ifndef STRUCTURES_H
#define STRUCTURES_H
const int SCATTERING_TEXTURE_R_SIZE = 32;
const int SCATTERING_TEXTURE_MU_SIZE = 128;
const int SCATTERING_TEXTURE_MU_S_SIZE = 32;
const int SCATTERING_TEXTURE_NU_SIZE = 8;

const int SCATTERING_TEXTURE_WIDTH =
    SCATTERING_TEXTURE_NU_SIZE * SCATTERING_TEXTURE_MU_S_SIZE;
const int SCATTERING_TEXTURE_HEIGHT = SCATTERING_TEXTURE_MU_SIZE;
const int SCATTERING_TEXTURE_DEPTH = SCATTERING_TEXTURE_R_SIZE;

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
    float cloudsStartRadius;

    vec3 solarIrradiance;
    float cloudsEndRadius;

    vec3 absorptionCoefficients; // Ozone layer
    float ozonePeakHeight;

    float ozoneTroposphereCoef;
    float ozoneTroposphereConst;
    float ozoneStratosphereCoef;
    float ozoneStratosphereConst;

    float cloudLayerThickness;// Should be precomputed as (cloudsEndRadius - cloudsStartRadius)
    float sunAngularRadius;
    float atmosphereThickness;// Should be precomputed as (atmosphereRadius - surfaceRadius)
    float padding2;
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