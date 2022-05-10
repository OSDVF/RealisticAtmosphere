/**
  * @author Ondøej Sabela
  * @brief Realistic Atmosphere - Thesis implementation.
  * @date 2021-2022
  * Copyright 2022 Ondøej Sabela. All rights reserved.
  * Uses ray tracing, path tracing and ray marching to create visually plausible outdoor scenes with atmosphere, terrain, clouds and analytical objects.
  * Code reused from BGFX examples.
  */

#pragma once
#include "bgfx/bgfx.h"

struct PosTexCoordVertex
{
	float m_x;
	float m_y;
	float m_z;
	float m_u;
	float m_v;

	static void init()
	{
		ms_layout
			.begin()
			.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
			.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
			.end();
	}

	static inline bgfx::VertexLayout ms_layout;
};