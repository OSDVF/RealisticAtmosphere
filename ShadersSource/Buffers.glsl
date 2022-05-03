//?#version 440
//!#define DEBUG
#ifndef BUFFERS
#define BUFFERS
#include "SceneObjects.glsl"
#include "Debug.glsl"
layout(std430, binding=3) readonly buffer ObjectBuffer
{
    Sphere objects[];
};

layout(std430, binding=4) readonly buffer PlanetBuffer 
{
    Planet planets[];
};

layout(std430, binding=5) readonly buffer MaterialBuffer
{
    Material materials[];
};

layout(std430, binding=6) readonly buffer DirectionalLightBuffer
{
    DirectionalLight directionalLights[];
};
#endif