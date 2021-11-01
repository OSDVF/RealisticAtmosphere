#pragma once
#include "bx/bx.h"
struct vec4 {
	float x;
	float y;
	float z;
	float w;
	vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
	float length()
	{
		return bx::sqrt(x * x + y * y + z * z + w * w);
	}
	vec4 normalize()
	{
		float len = length();
		return vec4(x / len, y / len, z / len, w / len);
	}
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

struct ivec4 {
	int x, y, z, w;
	ivec4(int x, int y, int z, int w): x(x), y(y), z(z), w(w)
	{
	}
};