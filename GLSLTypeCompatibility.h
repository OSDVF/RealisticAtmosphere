#pragma once
#include "bx/bx.h"
struct vec4 {
    float x;
    float y;
    float z;
    float w;
    vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
};
using vec3 = bx::Vec3;
struct vec2 {
    float x;
    float y;
    vec2(float x, float y)
    {
        this->x = x;
        this->y = y;
    }
};
using uint = uint32_t;