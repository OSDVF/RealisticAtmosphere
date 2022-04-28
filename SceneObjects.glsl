//?#version 430
//!#define BGFX_SHADER_LANGUAGE_GLSL
// FOR SOME REASON ALL UNIFORMS MUST BE DEFINED FIRST TO BE RECOGNIZED BY BGFX ANNOYING SHADERC

/**
 * Represents camera matrix (location, direction, up, right, fovX, fovY)
 */
#ifndef SCENEOBJECTS
#define SCENEOBJECTS
#define AMBIENT_LIGHT vec3(0)
#define HQFlags_NONE 0u
#define HQFlags_EARTH_SHADOWS 1u
#define HQFlags_ATMO_COMPUTE 2u
#define HQFlags_LIGHT_SHAFTS 4u
#define HQFlags_INDIRECT_APPROX 8u
#define HQFlags_REDUCE_BANDING 16u

#ifdef BGFX_SHADER_LANGUAGE_GLSL
const float POSITIVE_INFINITY = 3.402823466e+38;
const float NEGATIVE_INFINITY = uintBitsToFloat(0xFF800000);
#define SPACE_COLOR vec3(0.1,0.1,0.3)
uniform sampler2DArray terrainTextures;
uniform sampler2D heightmapTexture;
uniform sampler2D opticalDepthTable;
uniform sampler2D transmittanceTable;
uniform sampler2DArray irradianceTable;
uniform sampler3D singleScatteringTable;
uniform vec4 u_viewRect;
uniform vec4 Camera[4];
uniform vec4 MultisamplingSettings;
uniform vec4 QualitySettings;
uniform vec4 PlanetMaterial;
uniform vec4 RaymarchingSteps;
uniform vec4 HQSettings;
uniform vec4 LightSettings[3];
uniform vec4 SunRadianceToLuminance;
uniform vec4 SkyRadianceToLuminance;
uniform vec4 CloudsSettings[4];
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
vec4 QualitySettings = {5, 50, 63000, 0.4};
int currentSample = 0;
int directSamples = 1;//Direct samples per all samples
unsigned int flags = HQFlags_INDIRECT_APPROX | HQFlags_REDUCE_BANDING;
vec4 HQSettings = {*(float*)&flags, *(float*)&currentSample, *(float*)&directSamples, 1};
int lightTerrainDetectSteps = 40;
vec4 LightSettings[] = {
                            vec4(2000, 0.03, 0.4, -0.09),
                            vec4(0.5, *(float*)&lightTerrainDetectSteps, 3, 0.6),
                            vec4(2000, 700)
                        };
vec4 PlanetMaterial = {1700, 2300, .4, .75};
int planetSteps = 152;
vec4 RaymarchingSteps = {*(float*)&planetSteps, 0.01, 0.005, 0.5};
vec4 SunRadianceToLuminance;
vec4 SkyRadianceToLuminance;
vec4 CloudsSettings[] = {
                            vec4(128, 4, 200000, 1000),//samples, light samples, far plane, light far plane
                            vec4(50,  5, 0.001, 0),//Terrain steps, cheap downsamle, cheap thres, max powder
                            vec4(1e-4, 0.1, 0.3, 2),//sampling thres, aerosol amount, powder density, fade power
                            vec4(128, 200000, 8)// light shafts steps, light shafts far plane, occlusion power
                        };
#endif

#define Multisampling_indirect MultisamplingSettings.x
#define Multisampling_maxBounces MultisamplingSettings.y
#define Multisampling_perAtmospherePixel MultisamplingSettings.z
#define Multisampling_type MultisamplingSettings.w

#define QualitySettings_lodPow QualitySettings.x
#define QualitySettings_minStepSize QualitySettings.y
#define QualitySettings_farPlane QualitySettings.z
#define QualitySettings_optimism QualitySettings.w

#define LightSettings_farPlane LightSettings[0].x
#define LightSettings_precision LightSettings[0].y
#define LightSettings_noRayThres LightSettings[0].z
#define LightSettings_viewThres LightSettings[0].w
#define LightSettings_cutoffDist LightSettings[1].x
#define LightSettings_shadowSteps LightSettings[1].y
#define LightSettings_gradient LightSettings[1].z
#define LightSettings_terrainOptimMult LightSettings[1].w
#define LightSettings_shadowCascade LightSettings[2].x
#define LightSettings_bandingFactor LightSettings[2].y

#define HQSettings_flags HQSettings.x
#define HQSettings_sampleNum HQSettings.y
#define HQSettings_directSamples HQSettings.z
// Used for defining exposure in Exposure tonemapping and the white point in Reinhard2 tonemapping
#define HQSettings_exposure HQSettings.w

#define Camera_position (Camera[0].xyz)
#define Camera_direction (Camera[1].xyz)
#define Camera_up (Camera[2].xyz)
#define Camera_right (Camera[3].xyz)
#define Camera_fovX (Camera[3].w)
#define Camera_fovY (Camera[2].w)

#define Clouds_iter CloudsSettings[0].x
#define Clouds_lightSteps CloudsSettings[0].y
#define Clouds_farPlane CloudsSettings[0].z
#define Clouds_lightFarPlane CloudsSettings[0].w

#define Clouds_terrainSteps CloudsSettings[1].x
#define Clouds_cheapDownsample CloudsSettings[1].y
#define Clouds_cheapThreshold CloudsSettings[1].z
#define Clouds_maxPowder CloudsSettings[1].w

#define Clouds_sampleThres CloudsSettings[2].x
#define Clouds_aerosols CloudsSettings[2].y
#define Clouds_powderDensity CloudsSettings[2].z
#define Clouds_fadePower CloudsSettings[2].w

#define Clouds_occlusionSteps CloudsSettings[2].x
#define Clouds_occlusionFarPlane CloudsSettings[2].y
#define Clouds_occlusionPower CloudsSettings[2].z

#define CurrentChunk_x floatBitsToInt(SunRadianceToLuminance.w)
#define CurrentChunk_y floatBitsToInt(SkyRadianceToLuminance.w)

#ifdef BGFX_SHADER_LANGUAGE_GLSL
bool HQSettings_atmoCompute = (floatBitsToUint(HQSettings_flags) & HQFlags_ATMO_COMPUTE) != 0u;
bool HQSettings_earthShadows = (floatBitsToUint(HQSettings_flags) & HQFlags_EARTH_SHADOWS) != 0u;
bool HQSettings_lightShafts = (floatBitsToUint(HQSettings_flags) & HQFlags_LIGHT_SHAFTS) != 0u;
bool HQSettings_indirectApprox = (floatBitsToUint(HQSettings_flags) & HQFlags_INDIRECT_APPROX) != 0u;
bool HQSettings_reduceBanding = (floatBitsToUint(HQSettings_flags) & HQFlags_REDUCE_BANDING) != 0u;
#endif

#include "Structures.glsl"
#endif