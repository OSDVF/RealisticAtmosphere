/**
 * @author Ondøej Sabela
 * @brief Realistic Atmosphere - Thesis implementation.
 * @date 2021-2022
 * Copyright 2022 Ondøej Sabela. All rights reserved.
 * Uses ray tracing, path tracing and ray marching to create visually plausible outdoor scenes with atmosphere, terrain, clouds and analytical objects.
 * Large portions of source code reused from reference implementation of the Precomputed Atmospheric Scattering paper by Eric Bruneton and Fabrice Neyret.
 * Code available at https://github.com/ebruneton/precomputed_atmospheric_scattering.
 */

//?#version 440
#ifndef PRECOMPUTE_H_
#define PRECOMPUTE_H_

uniform vec4 PrecomputeSettings = vec4(0,0,0,0);
#define PrecomputeSettings_planetIndex floatBitsToUint(PrecomputeSettings.x)
#define PrecomputeSettings_lightIndex floatBitsToUint(PrecomputeSettings.y)
#endif