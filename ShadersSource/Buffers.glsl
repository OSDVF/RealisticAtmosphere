/**
 * @author Ondøej Sabela
 * @brief Realistic Atmosphere - Thesis implementation.
 * @date 2021-2022
 * Copyright 2022 Ondøej Sabela. All rights reserved.
 * Uses ray tracing, path tracing and ray marching to create visually plausible outdoor scenes with atmosphere, terrain, clouds and analytical objects.
 */

//?#version 440
//!#define DEBUG
#ifndef BUFFERS
#define BUFFERS
#include "SceneObjects.glsl"
#include "Debug.glsl"
layout(std430, binding=3) readonly buffer ObjectBuffer
{
    AnalyticalObject objects[];
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