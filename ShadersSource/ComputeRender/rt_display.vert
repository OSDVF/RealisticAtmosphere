/**
 * @author Ondøej Sabela
 * @brief Realistic Atmosphere - Thesis implementation.
 * @date 2021-2022
 * Copyright 2022 Ondøej Sabela. All rights reserved.
 * Uses ray tracing, path tracing and ray marching to create visually plausible outdoor scenes with atmosphere, terrain, clouds and analytical objects.
 */

//!#version 440
in vec3 a_position;
in vec2 a_texcoord0;

out vec2 texCoord;
uniform mat4 u_modelViewProj;
void main()
{
    texCoord = a_texcoord0;
    gl_Position = u_modelViewProj * vec4(a_position, 1.0);
}