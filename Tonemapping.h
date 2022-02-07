#ifndef  TONEMAPPNG_H
#define TONEMAPPING_H
float tmFunc(float hdrColor)
{
	return hdrColor < 1.4131 * HQSettings_exposure ? /*gamma correction*/ pow(hdrColor * 0.38317, 1.0 / 2.2) : 1.0 - exp(-hdrColor * HQSettings_exposure)/*exposure tone mapping*/;
}
#endif // ! TONEMAPPNG_H

