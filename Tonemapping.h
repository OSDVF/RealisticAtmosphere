#ifndef  TONEMAPPNG_H
#define TONEMAPPING_H
float tmScratchFunc(float hdrColor)
{
	return hdrColor < 1.4131 * HQSettings_exposure ? /*gamma correction*/ pow(hdrColor * 0.38317, 1.0 / 2.2) : 1.0 - exp(-hdrColor * HQSettings_exposure)/*exposure tone mapping*/;
}
//Tonemapping function
float tmFunc(float hdrColor)
{
	return pow(1.0 - exp(-hdrColor * HQSettings_exposure), 1.0 / 2.2);
}
#endif // ! TONEMAPPNG_H

