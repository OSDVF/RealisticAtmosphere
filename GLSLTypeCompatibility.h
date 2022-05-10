/**
  * @author Ondøej Sabela
  * @brief Realistic Atmosphere - Thesis implementation.
  * @date 2021-2022
  * Copyright 2022 Ondøej Sabela. All rights reserved.
  * Uses ray tracing, path tracing and ray marching to create visually plausible outdoor scenes with atmosphere, terrain, clouds and analytical objects.
  */

#pragma once
#include "bx/bx.h"
#include "bx/math.h"
using vec3 = bx::Vec3;
struct vec4 {
	float x;
	float y;
	float z;
	float w;
	vec4(float x = 0, float y = 0, float z = 0, float w = 0) : x(x), y(y), z(z), w(w) {}
	float length()
	{
		return bx::sqrt(x * x + y * y + z * z + w * w);
	}
	vec4 normalize()
	{
		float len = length();
		return vec4(x / len, y / len, z / len, w / len);
	}
	static inline vec4 fromVec3(vec3 v, float fourtComponent = 0)
	{
		return vec4(v.x, v.y, v.z, fourtComponent);
	}
	vec3 toVec3()
	{
		return vec3(this->x, this->y, this->z);
	}
};
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

struct ivec4 {
	int x, y, z, w;
	ivec4(int x, int y, int z, int w) : x(x), y(y), z(z), w(w)
	{
	}
};