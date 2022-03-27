#ifndef  TONEMAPPNG_H
#ifndef BGFX_SHADER_LANGUAGE_GLSL
#include <glm/glm.hpp>
namespace Tonemapping {
	using namespace glm;
#endif
#define TONEMAPPING_H

	float exposure(float hdrColor)
	{
		return 1.0 - exp(-hdrColor * HQSettings_exposure);
	}

	float gammaThenExposure(float hdrColor)
	{
		return hdrColor < 1.4131 * HQSettings_exposure ? /*gamma correction*/ pow(hdrColor * 0.38317, 1.0 / 2.2) : 1.0 - exp(-hdrColor * HQSettings_exposure)/*exposure tone mapping*/;
	}

	//https://www.shadertoy.com/view/WdjSW3

	float Reinhard(float x) {
		return x / (1.0 + x);
	}

	float Reinhard2(float x) {
		const float L_white = 4.0;
		return (x * (1.0 + x / (L_white * L_white))) / (1.0 + x);
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
		const float P = 1.0;  // max display brightness
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
		const float hdrMax = 8.0;
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
		return pow(Tonemap_ACES(hdrColor), 1.0 / g);
	}

	float gamma(float hdrColor)
	{
		return gamma(hdrColor, 2.2);
	}

	float tmFunc(float hdrColor, int tonemappingType)
	{
		switch (tonemappingType)
		{
		case 1:
			return gammaThenExposure(hdrColor);
		case 2:
			return gamma(Reinhard(hdrColor));
		case 3:
			return gamma(Reinhard2(Tonemap_ACES(hdrColor)));
		case 4:
			return gamma(Tonemap_Unreal(hdrColor));
		case 5:
			return gamma(Tonemap_Uchimura(hdrColor));
		case 6:
			return gamma(Tonemap_Uchimura(hdrColor));
		case 7:
			return gamma(Tonemap_Lottes(hdrColor));
		default:
			return gamma(exposure(hdrColor));
		}
	}

#ifndef BGFX_SHADER_LANGUAGE_GLSL
};
#endif
#endif // ! TONEMAPPNG_H

