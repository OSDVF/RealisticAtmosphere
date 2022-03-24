#pragma once
#include "GLSLTypeCompatibility.h"
#include "Structures.glsl"
#include <array>
/**
* Contains global data of currently submitted scene objects
*/
namespace DefaultScene
{
	const Material materialBuffer[] = {
	{
		{1,1,1,0},// White transparent
		{0,0,0},// No Specular part
		{0},// No Roughness
		{20,20,20}, // Max emission
		0
	},
	{
		{1, 1, 1, 1},// White albedo
		{0.5,0.5,0.5},// Half specular
		{0.5},// Half Roughness
		{0,0,0}, // No emission
		0
	},
	{
		{0, 0, 1, 1},// Blue albedo
		{0.5,0.5,0.5},// Half specular
		{0.5},// Half Roughness
		{0,0,0}, // No emission
		0
	}
	};

	const float earthRadius = 6360000; // cit. E. Bruneton page 3
	const float cloudsStart = earthRadius + 500;
	const float cloudsEnd = earthRadius + 20000;
	const float atmosphereRadius = 6420000;
	const double sunAngularRadius = 0.00935 / 2.0;
	const float moonAngularRadiusMin = 0.00855211333f;
	const float moonAngularRadiusMax = 0.00973893723f;
	const float sunObjectDistance = 148500000; // Real sun distance is 148 500 000 km which would introduce errors in 32bit float computations
	const float sunRadius = tan(sunAngularRadius) * sunObjectDistance;
	const float moonRadius = tan(moonAngularRadiusMax) * sunObjectDistance;
	Sphere objectBuffer[] = {
		{//Sun
			{0, 0, sunObjectDistance}, //Position
			{sunRadius}, //Radius
			0 //Material index
		},
		{//Moon
			{0, 0, sunObjectDistance}, //Position
			{moonRadius}, //Radius
			0 //Material index
		},
		{
			{-18260, 1706, 16065}, //Position
			{1}, //Radius
			1, //Material index
		},
		{
			{-18260, 1706, 16065.7}, //Position
			{1}, //Radius
			2, //Material index
		}
	};

	std::array<DirectionalLight, 2> directionalLightBuffer = {
		DirectionalLight{//Sun
			{0,0,0},//Direction will be assigned
			float(sunAngularRadius),
			{1,1,1},//irradiance will be assigned
			0.3
		},
		DirectionalLight{//Moon
			{0,0,0},//Direction will be assigned
			moonAngularRadiusMax,
			{
				0.005,
				0.005,
				0.005
			},
		//https://www.ncbi.nlm.nih.gov/pmc/articles/PMC4487308/
		//values are in μW.m−2.nm−1 so we need to convert them to lumens
		1
	}
	};

	std::array<Planet, 1> planetBuffer = {
		Planet
		{
			vec3(0, -earthRadius, 0),//center
			earthRadius,//start radius

			atmosphereRadius,//end radius
			0,//βˢₘ Will be assigned later
			0.8,//Mie asymmetry factor
			1200,//Mie scale height
			vec3(0,0,0),//βˢᵣ Will be assigned later
			7994,//Rayleigh scale height

			vec3(0,0,0),//Absorption (ozone) extinction coefficients - will be assigned later
			earthRadius + 5000, // Mountains radius

			atmosphereRadius - earthRadius, // Atmosphere thickness
			25000,//Ozone peak height - height at which the ozone has maximum relative density
			(1.0 / 15000.0),//Ozone troposphere density coefficient - for heights below ozonePeakHeight
			-(1.0 / 15000.0),//Ozone stratosphere density coefficient - for heights above peak

			-(1.0),//Ozone troposphere density constant
			(7.0 / 3.0),//Ozone stratosphere density constant
			0,//First light index (sun = 0)
			0,//Last light index (moon would be 1)
			CloudLayer
			{
				{-23911, 0, 20000}, // position
				0.09,//coverage

				cloudsStart, // Clouds start radius
				cloudsEnd, // Clouds end radius
				cloudsEnd - cloudsStart, // Clouds layer thickness

				3,
				5000,//Thickness of clouds fade gradient above terrain
				13000, //Thickness of clouds gradient below stratosphere
				0.000855 * 0.999,// Scattering coefficient = ext. coef * single scat.albedo
				0.000855,// Extinction coefficient

				{1e-4,2e-4,1e-4},//size
				5 // sharpness
			},
			0,//First light index in precomputed textures
			0,0,0//Padding
		},
	};
};