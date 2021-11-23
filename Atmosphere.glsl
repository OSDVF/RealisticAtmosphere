//?#version 440
#include "Buffers.glsl"
#include "Intersections.glsl"
#define PI (3.14159265358979323846) 

vec3 atmosphereColor(Atmosphere atmosphere, Ray ray, float minDistance, float maxDistance)
{
	float t0, t1;
	ray.direction = normalize(ray.direction);
	if(!raySphereIntersection(atmosphere.center, atmosphere.endRadius, ray, t0, t1)
		|| (t1 < 0 ))// this would mean that the atmosphere is behind us
	{
		return AMBIENT_LIGHT;
	}

	//Limit atmosphere bounds according to the Hit
	if(t0 > minDistance && t0 > 0) minDistance = t0;
	if(t1 < maxDistance) maxDistance = t1;

	float segmentLength = (maxDistance - minDistance) / Multisampling_perAtmospherePixel;
	float currentDistance = minDistance;

	vec3 rayleighColor = vec3(0);
	vec3 mieColor = vec3(0);
	float opticalDepthR = 0, opticalDepthM = 0; 

	vec3 sunVector = normalize(objects[atmosphere.sunObjectIndex].position - atmosphere.center); /*origin at planet, point to sun*/
	float angleDot = dot(sunVector, ray.direction);

	float rayleightPhase = 3.0 / (16.0 * PI) * (1 + angleDot * angleDot);
	float assymetryFactor2 = atmosphere.mieAsymmetryFactor * atmosphere.mieAsymmetryFactor;
	float miePhase = 3.0 /
		(8.0 * PI) * ((1.0 - assymetryFactor2) * (1.0 + (angleDot * angleDot)))
		/ ((2.f + assymetryFactor2) * pow(1.0 + assymetryFactor2 - 2.0 * atmosphere.mieAsymmetryFactor * angleDot, 1.5f));

	for(int i = 0; i < Multisampling_perAtmospherePixel; i++)
	{
		vec3 worldSamplePos = ray.origin + (currentDistance + segmentLength * 0.5) * ray.direction;
		float centerDist = distance(worldSamplePos, atmosphere.center);
		// Height above the sea level
		float sampleHeight = centerDist - atmosphere.startRadius;

		//Compute optical depth; HF = height factor
		float rayleighHF = exp(-sampleHeight/atmosphere.rayleighScaleHeight) * segmentLength;
		float mieHF = exp(-sampleHeight/atmosphere.mieScaleHeight) * segmentLength;
		opticalDepthR += rayleighHF; 
        opticalDepthM += mieHF; 

		//Compute light optical depth
        float t0Light, t1Light; 
		// Intersect light ray with outer shell of the atmosphere
        raySphereIntersection(atmosphere.center, atmosphere.endRadius, Ray(worldSamplePos, sunVector), t0Light, t1Light); 
        float lSegmentLength = t1Light / Multisampling_perLightRay;
		float tCurrentLight = 0; 
        float lOpticalDepthR = 0, lOpticalDepthM = 0; 

		int l;
		for (l = 0; l < Multisampling_perLightRay; ++l) { 
            vec3 lSamplePos = worldSamplePos + (tCurrentLight + lSegmentLength * 0.5) * sunVector; 
			float lCenterDist = distance(lSamplePos, atmosphere.center);
            float lSampleHeight = lCenterDist - atmosphere.startRadius; 
            if (lSampleHeight < 0) break;

            lOpticalDepthR += exp(-lSampleHeight / atmosphere.rayleighScaleHeight) * lSegmentLength; 
            lOpticalDepthM += exp(-lSampleHeight / atmosphere.mieScaleHeight) * lSegmentLength; 
            tCurrentLight += lSegmentLength; 
        }
		if(l == Multisampling_perLightRay)
		{
			//Finalize the computation and commit to the result color

			//There should be extinction coefficients, but for Rayleigh, they are the same as scattering coeff.s and for Mie, it is 1.11 times the s.c.
			float mieExtinction = 1.11 * atmosphere.mieCoefficient;
			vec3 depth = atmosphere.rayleighCoefficients * (lOpticalDepthR + opticalDepthR)
							+ mieExtinction * (lOpticalDepthM + opticalDepthM);
			vec3 attenuation = exp(-depth);

			rayleighColor += attenuation * rayleighHF;
			mieColor += attenuation * mieHF;
		}
		// Shift to next sample
		currentDistance += segmentLength;
	}
	return (rayleighColor * atmosphere.rayleighCoefficients * rayleightPhase
			+ mieColor * atmosphere.mieCoefficient * miePhase
			) * atmosphere.sunIntensity;
}