/**
 * @author Ondøej Sabela
 * @brief Realistic Atmosphere - Thesis implementation.
 * @date 2021-2022
 * Copyright 2022 Ondøej Sabela. All rights reserved.
 * Uses ray tracing, path tracing and ray marching to create visually plausible outdoor scenes with atmosphere, terrain, clouds and analytical objects
 */

#include "ScreenSpaceQuad.h"
ScreenSpaceQuad::ScreenSpaceQuad(float _textureWidth, float _textureHeight, float _width, float _height)
{
	_caps = bgfx::getCaps();
	_texelHalf = bgfx::RendererType::Direct3D9 == _caps->rendererType ? 0.5f : 0.0f;

	PosTexCoordVertex::init();

	const float zz = 0.0f;

	const float minx = -_width;
	const float maxx = _width;
	const float miny = 0.0f;
	const float maxy = _height * 2.0f;

	const float texelHalfW = _texelHalf / _textureWidth;
	const float texelHalfH = _texelHalf / _textureHeight;
	const float minu = -1.0f + texelHalfW;
	const float maxu = 1.0f + texelHalfW;

	float minv = texelHalfH;
	float maxv = 2.0f + texelHalfH;

	if (_caps->originBottomLeft)
	{
		float temp = minv;
		minv = maxv;
		maxv = temp;

		minv -= 1.0f;
		maxv -= 1.0f;
	}

	_vertices[0].m_x = minx;
	_vertices[0].m_y = miny;
	_vertices[0].m_z = zz;
	_vertices[0].m_u = minu;
	_vertices[0].m_v = minv;

	_vertices[1].m_x = maxx;
	_vertices[1].m_y = miny;
	_vertices[1].m_z = zz;
	_vertices[1].m_u = maxu;
	_vertices[1].m_v = minv;

	_vertices[2].m_x = maxx;
	_vertices[2].m_y = maxy;
	_vertices[2].m_z = zz;
	_vertices[2].m_u = maxu;
	_vertices[2].m_v = maxv;

	_vb = bgfx::createVertexBuffer(
		// Static data can be passed with bgfx::makeRef
		bgfx::makeRef(_vertices, sizeof(_vertices)), PosTexCoordVertex::ms_layout
	);
}
ScreenSpaceQuad::ScreenSpaceQuad()
{
}
void ScreenSpaceQuad::draw()
{
	bgfx::setVertexBuffer(0, _vb);
}

void ScreenSpaceQuad::destroy()
{
	bgfx::destroy(_vb);
}