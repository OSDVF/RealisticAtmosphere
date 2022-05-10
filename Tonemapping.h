/**
  * @author Ondøej Sabela
  * @brief Realistic Atmosphere - Thesis implementation.
  * @date 2021-2022
  * Copyright 2022 Ondøej Sabela. All rights reserved.
  * Uses ray tracing, path tracing and ray marching to create visually plausible outdoor scenes with atmosphere, terrain, clouds and analytical objects.
  * Tonemapping operators are taken from example at ShaderToy (link is in the source code)
  * and BGFX shader library at https://github.com/bkaradzic/bgfx/blob/master/examples/common/shaderlib.sh
  */

// This file can be included both in C++ and GLSL code

#ifndef  TONEMAPPNG_H
#ifndef BGFX_SHADER_LANGUAGE_GLSL
#include <glm/glm.hpp>
namespace Tonemapping {
	using namespace glm;
	using vec3 = glm::vec3;
#endif
#define TONEMAPPING_H
	
	// Tonemapping must be done in CIE XYZ space
	// There are some conversion functions
	vec3 convertRGB2XYZ(vec3 _rgb)
	{
		// Reference(s):
		// - RGB/XYZ Matrices
		//   https://web.archive.org/web/20191027010220/http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html
		vec3 xyz;
		xyz.x = dot(vec3(0.4124564, 0.3575761, 0.1804375), _rgb);
		xyz.y = dot(vec3(0.2126729, 0.7151522, 0.0721750), _rgb);
		xyz.z = dot(vec3(0.0193339, 0.1191920, 0.9503041), _rgb);
		return xyz;
	}

	vec3 convertXYZ2RGB(vec3 _xyz)
	{
		vec3 rgb;
		rgb.x = dot(vec3(3.2404542, -1.5371385, -0.4985314), _xyz);
		rgb.y = dot(vec3(-0.9692660, 1.8760108, 0.0415560), _xyz);
		rgb.z = dot(vec3(0.0556434, -0.2040259, 1.0572252), _xyz);
		return rgb;
	}

	vec3 convertXYZ2Yxy(vec3 _xyz)
	{
		// Reference(s):
		// - XYZ to xyY
		//   https://web.archive.org/web/20191027010144/http://www.brucelindbloom.com/index.html?Eqn_XYZ_to_xyY.html
		float inv = 1.0 / dot(_xyz, vec3(1.0, 1.0, 1.0));
		return vec3(_xyz.y, _xyz.x * inv, _xyz.y * inv);
	}

	vec3 convertYxy2XYZ(vec3 _Yxy)
	{
		// Reference(s):
		// - xyY to XYZ
		//   https://web.archive.org/web/20191027010036/http://www.brucelindbloom.com/index.html?Eqn_xyY_to_XYZ.html
		vec3 xyz;
		xyz.x = _Yxy.x * _Yxy.y / _Yxy.z;
		xyz.y = _Yxy.x;
		xyz.z = _Yxy.x * (1.0 - _Yxy.y - _Yxy.z) / _Yxy.z;
		return xyz;
	}

	vec3 convertRGB2Yxy(vec3 _rgb)
	{
		return convertXYZ2Yxy(convertRGB2XYZ(_rgb));
	}

	vec3 convertYxy2RGB(vec3 _Yxy)
	{
		return convertXYZ2RGB(convertYxy2XYZ(_Yxy));
	}

	float exposure(float hdrColor)
	{
		return 1.0 - exp(-hdrColor * HQSettings_exposure);
	}

	float gammaThenExposure(float hdrColor)
	{
		return hdrColor < 1.4131 * HQSettings_exposure ? /*gamma correction*/ pow(hdrColor * 0.38317 * HQSettings_exposure, 1.0 / 2.2) : 1.0 - exp(-hdrColor * HQSettings_exposure)/*exposure tone mapping*/;
	}

	// Tonemapping operators are from
	// https://www.shadertoy.com/view/WdjSW3
	float Reinhard(float x) {
		return x / (1.0 + x);
	}

	float Reinhard2(float x) {
		return (x * (1.0 + x / (HQSettings_exposure * HQSettings_exposure))) / (1.0 + x);
	}

	float Tonemap_ACES(float x) {
		// Narkowicz 2015, "ACES Filmic Tone Mapping Curve"
		const float a = 2.51;
		const float b = 0.03;
		const float c = 2.43;
		const float d = 0.59;
		const float e = 0.14;
		return (x * (a * x + b)) / (x * (c * x + d) + e);
	}

	float Tonemap_Unreal(float x) {
		// Unreal 3, Documentation: "Color Grading"
		// Adapted to be close to Tonemap_ACES, with similar range
		// Gamma 2.2 correction is baked in, don't use with sRGB conversion!
		return x / (x + 0.155) * 1.019;
	}

	float Tonemap_Uchimura(float x, float P, float a, float m, float l, float c, float b) {
		// Uchimura 2017, "HDR theory and practice"
		// Math: https://www.desmos.com/calculator/gslcdxvipg
		// Source: https://www.slideshare.net/nikuque/hdr-theory-and-practicce-jp
		float l0 = ((P - m) * l) / a;
		float L0 = m - m / a;
		float L1 = m + (1.0 - m) / a;
		float S0 = m + l0;
		float S1 = m + a * l0;
		float C2 = (a * P) / (P - S1);
		float CP = -C2 / P;

		float w0 = 1.0 - smoothstep(float(0.0), m, x);
		float w2 = step(m + l0, x);
		float w1 = 1.0 - w0 - w2;

		float T = m * pow(x / m, c) + b;
		float S = P - (P - S1) * exp(CP * (x - S0));
		float L = m + a * (x - m);

		return T * w0 + L * w1 + S * w2;
	}

	float Tonemap_Uchimura(float x) {
		const float P = HQSettings_exposure;  // max display brightness
		const float a = 1.0;  // contrast
		const float m = 0.22; // linear section start
		const float l = 0.4;  // linear section length
		const float c = 1.33; // black
		const float b = 0.0;  // pedestal
		return Tonemap_Uchimura(x, P, a, m, l, c, b);
	}

	float Tonemap_Lottes(float x) {
		// Lottes 2016, "Advanced Techniques and Optimization of HDR Color Pipelines"
		const float a = 1.6;
		const float d = 0.977;
		const float hdrMax = HQSettings_exposure;
		const float midIn = 0.18;
		const float midOut = 0.267;

		// Can be precomputed
		const float b =
			(-pow(midIn, a) + pow(hdrMax, a) * midOut) /
			((pow(hdrMax, a * d) - pow(midIn, a * d)) * midOut);
		const float c =
			(pow(hdrMax, a * d) * pow(midIn, a) - pow(hdrMax, a) * pow(midIn, a * d) * midOut) /
			((pow(hdrMax, a * d) - pow(midIn, a * d)) * midOut);

		return pow(x, a) / (pow(x, a * d) * b + c);
	}

	float gamma(float hdrColor, float g)
	{
		return pow(hdrColor, 1.0 / g);
	}

	float gamma(float hdrColor)
	{
		return gamma(hdrColor, 2.2);
	}

	vec3 gammaRGB(vec3 hdrColor)
	{
		return vec3(gamma(hdrColor.x), gamma(hdrColor.y), gamma(hdrColor.z));
	}

	vec3 tmFunc(vec3 rgb, int tonemappingType)
	{
		vec3 Yxy = convertRGB2Yxy(rgb);
		switch (tonemappingType)
		{
		case 0:
			Yxy.x = exposure(Yxy.x);
			break;
		case 1:
			Yxy.x = Reinhard(Yxy.x);
			break;
		case 2:
			Yxy.x = Reinhard2(Yxy.x);
			break;
		case 3:
			Yxy.x = Tonemap_ACES(Yxy.x);
			break;
		case 4:
			Yxy.x = pow(Tonemap_Unreal(Yxy.x), 2.2);//We must do reverse gamma correction
			break;
		case 5:
			Yxy.x = Tonemap_Uchimura(Yxy.x);
			break;
		case 6:
			Yxy.x = Tonemap_Lottes(Yxy.x);
			break;
		default:
			return gammaRGB(rgb);
		}

		return gammaRGB(convertYxy2RGB(Yxy));
	}

#ifndef BGFX_SHADER_LANGUAGE_GLSL
};
#endif
#endif // ! TONEMAPPNG_H

