/**
  * @author Ondøej Sabela
  * @brief Realistic Atmosphere - Thesis implementation.
  * @date 2021-2022
  * Copyright 2022 Ondøej Sabela. All rights reserved.
  * Uses ray tracing, path tracing and ray marching to create visually plausible outdoor scenes with atmosphere, terrain, clouds and analytical objects.
  */

#pragma once
#include "bgfx/bgfx.h"
#include "PosTexCoordVertex.h"
class ScreenSpaceQuad
{
public: 
	static inline float _texelHalf = 0.0f;
	static inline PosTexCoordVertex _vertices[3];
	const bgfx::Caps* _caps;
	bgfx::VertexBufferHandle _vb;

	ScreenSpaceQuad(float _textureWidth, float _textureHeight, float _width = 1.0f, float _height = 1.0f);
	ScreenSpaceQuad();

	void draw();
	void destroy();
};

