#pragma once
#include "GLSLTypeCompatibility.h"
#include "ShadersSource/Structures.glsl"
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
			{1,1,1}, // Emission will be assigned
		},
		{
			{1, 1, 1, 1},// White albedo
			{0,0,0}, // No emission
		},
		{
			{0, 0, 1, 1},// Blue albedo
			{0,0,0}, // No emission
		},
		{
			{1, 0.5, 0.15, 1},//Orange albedo
			{0}
		},
		{	//Green
			{0.1, 0.9, 0.1, 1},
			{0}
		},
		{	//Red
			{0.9, 0.1, 0.1, 1},
			{0}
		},
		{
			//Gray
			{0.7, 0.7, 0.7, 1},
			{0}
		},
		{
			//Dark
			{38.0/255.0, 25.0/255.0, 17.0/255.0, 1},
			{0}
		},
	};

	const float earthRadius = 6360000; // cit. E. Bruneton page 3
	const float mountainRadius = earthRadius + 4000;
	const float cloudsStart = earthRadius + 500;
	const float cloudsEnd = earthRadius + 20000;
	const float atmosphereRadius = 6420000;
	const double sunAngularRadius = 0.00935 / 2.0;
	const float moonAngularRadiusMin = 0.00855211333f;
	const float moonAngularRadiusMax = 0.00973893723f;
	const float sunObjectDistance = 148500000; // Real sun distance is 148 500 000 km which would introduce errors in 32bit float computations
	const float sunRadius = tan(sunAngularRadius) * sunObjectDistance;
	const float moonRadius = tan(moonAngularRadiusMax) * sunObjectDistance;
	AnalyticalObject objectBuffer[] = {
		{//Sun
			{0, 0, sunObjectDistance}, //Position
			0, //type = sphere
			{sunRadius, 0, 0}, //Radius
			0 //Material index
		},
		{//Blue sphere
			{22832, 2022, 13326}, //Position
			0, //sphere
			{1, 0, 0}, //Radius
			1, //Material index
		},
		{//White box
			{22832.2, 2022, 13326.7}, //Position
			1,
			{2, 2, 2}, //Size
			2, //Material index
		},
		{//Orange sphere - to be placed by user
			{0, 0, 0},
			0, // type = sphere
			{1, 0, 0},
			3
		},
		{//White box - to be placed by user
			{0, 0, 0},
			1, // type = box
			{1, 2, 0.5},
			2
		},
		//Cornell box:
		{
			//Left red plane
			{26630, 1336, 10749},
			1,
			{1,1,1},
			5
		},
		{
			//Right green plane
			{26632, 1336, 10749},
			1,
			{1,1,1},
			4
		},
		{
			//Bottom gray plane
			{26630, 1335, 10749},
			1,
			{3,1,1},
			6
		},
		{
			//Background gray plane
			{26630, 1336, 10750},
			1,
			{3,1,1},
			6
		},
		{
			//Left dark ball
			{26631.33, 1336.12, 10749.66},
			0,
			{0.12,0,0},
			7
		},
		{
			//Right orange ball
			{26631.66, 1336.12, 10749.66},
			0,
			{0.12,0,0},
			3
		},
		{
			//Front white ball
			{26631.5, 1336.12, 10749.33},
			0,
			{0.12,0,0},
			1
		}
	};

	std::array<DirectionalLight, SCATTERING_LIGHT_COUNT> directionalLightBuffer = {
		DirectionalLight{//Sun
			{0,0,0},//Direction will be assigned
			float(sunAngularRadius),
			{1,1,1},//irradiance will be assigned
			0
		},
#if SCATTERING_LIGHT_COUNT == 2
		DirectionalLight{//Moon - disabled
			{0,0,0},
			0,
			{0,0,0},
			0
		},
#endif
	};

	std::array<Planet, 1> planetBuffer = {
		Planet
		{//Aligned to vec4 multiplies
			vec3(0, -earthRadius, 0),//center
			earthRadius,//start radius

			atmosphereRadius,//end radius
			0,//κˢₘ Will be assigned later
			0.8,//Mie asymmetry factor
			1200,//Mie scale height

			vec3(0,0,0),//κˢᵣ Will be assigned later
			7994,//Rayleigh scale height

			vec3(0,0,0),//Absorption (ozone) extinction coefficients - will be assigned later
			mountainRadius, // Mountains radius

			mountainRadius - earthRadius, //Mountain height
			atmosphereRadius - earthRadius, // Atmosphere thickness
			25000,//Ozone peak height - height at which the ozone has maximum relative density
			(1.0 / 15000.0),//Ozone troposphere density coefficient - for heights below ozonePeakHeight

			-(2.0/3.0),//Ozone troposphere density constant
			-(1.0 / 15000.0),//Ozone stratosphere density coefficient - for heights above peak
			(8.0 / 3.0),//Ozone stratosphere density constant
			0,//First light index in precomputed textures

			0,//First light index (sun = 0)
			0,//Last light index (moon would be 1)

			-0.2,//mu_s_min
			0,//padding
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
				1.05 // sharpness
			},
		},
	};

	// Sun and camera position and rotation preset
	struct Preset {
		glm::vec3 camera;
		glm::vec3 rotation;
		struct {
			float x = 0;
			float y = 0;
		} sun;
		float cloudsFarPlane = 200000.0f;
	};

	const Preset presets[] = {
		Preset//0
		{
			{23796, 2266, 16636},
			{-4.962, -550, 0},
			{3.648, 1.484},
			150000
		},
		Preset//1
		{
			{22849, 2031, 13328},
			{-14, -601, 0},
			{3.648,  1.501}
		},
		Preset//2
		{
			{26632, 1337, 10748},
			{-5, -575, 0},
			{3.94,  1.46}
		},
		Preset//3
		{
			{0, 2000000, 0},
			{-50, -601, 0},
			{3.648,  1.501},
			4900000
		},
		Preset//4
		{
			{61950, 42398, -83371},
			{-34, -549, 0},
			{0,  0.593},
			200000
		},
		Preset//5
		{
			{25241, 29450, -104701},
			{-33, -545, 0},
			{0,  0.593},
			200000
		},
		Preset//6
		{
			{26631.5, 1336.5, 10747.5},
			{0, 0, 0},
			{3.94,  1.46}
		},
	};
};