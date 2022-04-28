﻿#pragma once
#include "GLSLTypeCompatibility.h"
#include "Structures.glsl"
#include <array>
/**
* Contains global data of currently submitted scene objects
*/
namespace DefaultScene
{
	Material materialBuffer[] = {
	{
			//Sun material
			{1,1,1,0},// White transparent
			{0,0,0},// No Specular part
			{0},// No Roughness
			{1,1,1}, // Emission will be assigned
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
		},
		{
			{1, 0.5, 0.15, 1},//Orange albedo
			{0,0,0},
			{0},
			{0,0,0},
			{0}
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
		{//Blue sphere
			{22832, 2022, 13326}, //Position
			{1}, //Radius
			1, //Material index
		},
		{//White sphere
			{22832.2, 2022, 13326.7}, //Position
			{1}, //Radius
			2, //Material index
		},
		{//Orange sphere - to be placed by user
			{0, 0, 0},
			{1},
			3
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
			earthRadius + 4000, // Mountains radius

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
				{-110000, -2000, 0}, // position
				//{-130000, 9701,-150000},//error check position
				0.005,//coverage

				cloudsStart, // Clouds start radius
				cloudsEnd, // Clouds end radius
				cloudsEnd - cloudsStart, // Clouds layer thickness

				1,//Density
				5000,//Thickness of clouds fade gradient above terrain
				5000, //Thickness of clouds gradient below stratosphere
				10.354e-3 * 0.9512,// Scattering coefficient = ext. coef * single scat.albedo
				10.354e-3,// Extinction coefficient

				{8e-5,16e-5,8e-5},//size
				1.4 // sharpness
			},
			0,//First light index in precomputed textures
			0,0,0//Padding
		},
	};

	struct Preset {
		glm::vec3 camera;
		glm::vec3 rotation;
		struct {
			float x = 0;
			float y = 0;
		} sun;
		float cloudsFarPlane = 200000.0f;
		struct {
			float x = 0;
			float y = 0;
		} moon;
	};

	const Preset presets[] = {
		Preset
		{
			{23796, 2266, 16636},
			{-4.962, -550, 0},
			{3.648, 1.484},
			150000
		},
		Preset
		{
			{22849, 2031, 13328},
			{-14, -601, 0},
			{3.648,  1.501}
		},
		Preset
		{
			{26632, 1337, 10748},
			{-5, -575, 0},
			{3.94,  1.46}
		},
		Preset
		{
			{0, 2000000, 0},
			{-50, -601, 0},
			{3.648,  1.501},
			4900000
		}
	};
};