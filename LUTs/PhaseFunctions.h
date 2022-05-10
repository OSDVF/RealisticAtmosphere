/**
  * @author Ondøej Sabela
  * @brief Realistic Atmosphere - Thesis implementation.
  * @date 2021-2022
  * Copyright 2022 Ondøej Sabela. All rights reserved.
  * Uses ray tracing, path tracing and ray marching to create visually plausible outdoor scenes with atmosphere, terrain, clouds and analytical objects.
  */


// Effective Mie phase functions computed by the MiePlot software
#pragma once
namespace PhaseFunctions {
	// More uniform water droplet size distributions tend to create coronas and brocken spectre
	// More disperse distributions tend to create halos, fogbows and glory

	extern const float CloudsRedUniform[1801];
	extern const float CloudsRedDisperse[1801];
	extern const float CloudsGreenUniform[1801];
	extern const float CloudsGreenDisperse[1801];
	extern const float CloudsBlueUniform[1801];
	extern const float CloudsBlueDisperse[1801];
};