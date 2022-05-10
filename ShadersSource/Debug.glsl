/**
 * @author Ondøej Sabela
 * @brief Realistic Atmosphere - Thesis implementation.
 * @date 2021-2022
 * Copyright 2022 Ondøej Sabela. All rights reserved.
 * Uses ray tracing, path tracing and ray marching to create visually plausible outdoor scenes with atmosphere, terrain, clouds and analytical objects.
 */

//?#version 440
#ifndef DEBUG_H
#define DEBUG_H
#ifdef DEBUG
    uniform vec4 debugAttributes;
    #define DEBUG_NORMALS (debugAttributes.x == 1.0)
    #define DEBUG_ALBEDO (debugAttributes.y == 1.0)
    #define DEBUG_ATMO_OFF (debugAttributes.z == 1.0)
#else
    #define DEBUG_ATMO_OFF false
    #define DEBUG_NORMALS false
    #define DEBUG_ALBEDO false
#endif
#endif