//?#version 440
#ifndef PRECOMPUTE_H_
#define PRECOMPUTE_H_

uniform vec4 PrecomputeSettings = vec4(0,0,0,0);
#define PrecomputeSettings_planetIndex floatBitsToUint(PrecomputeSettings.x)
#define PrecomputeSettings_lightIndex floatBitsToUint(PrecomputeSettings.y)
#endif