#pragma once
#include "bgfx/bgfx.h"
struct PosVertex
{
	float m_x;
	float m_y;
	float m_z;

	static void init()
	{
		ms_layout
			.begin()
			.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
			.end();
	};

	static inline bgfx::VertexLayout ms_layout;
};

