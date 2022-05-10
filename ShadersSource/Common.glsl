/**
 * @author Ondøej Sabela
 * @brief Realistic Atmosphere - Thesis implementation.
 * @date 2021-2022
 * Copyright 2022 Ondøej Sabela. All rights reserved.
 * Uses ray tracing, path tracing and ray marching to create visually plausible outdoor scenes with atmosphere, terrain, clouds and analytical objects.
 * Large portions of source code reused from reference implementation of the Precomputed Atmospheric Scattering paper by Eric Bruneton and Fabrice Neyret.
 * Code available at https://github.com/ebruneton/precomputed_atmospheric_scattering/blob/master/atmosphere/functions.glsl.
 */

 /**
 * Original License:
 * Copyright (c) 2017 Eric Bruneton
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Precomputed Atmospheric Scattering
 * Copyright (c) 2008 INRIA
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

//?#version 440
#ifndef COMMON_H
#define COMMON_H
#include "Structures.glsl"
#include "Math.glsl"

float ClampCosine(float mu) {
  return clamp(mu, -1.0, 1.0);
}

float ClampDistance(float d) {
  return max(d, 0.0);
}

float ClampRadius(Planet planet, float r) {
  return clamp(r, planet.surfaceRadius, planet.atmosphereRadius);
}

float SafeSqrt(float a) {
  return sqrt(max(a, 0.0));
}

/*Maps from [0,1] to [-0.5, 1.5]*/
float GetTextureCoordFromUnitRange(float x, float texture_size) {
  return 0.5 / texture_size + x * (1.0 - 1.0 / texture_size);
}

float GetUnitRangeFromTextureCoord(float u, float texture_size) {
  return (u - 0.5 / texture_size) / (1.0 - 1.0 / texture_size);
}

vec3 GetIrradiance(
    const Planet planet,
    IRRADIANCE_SAMPLER_TYPE irradiance_texture,
    float r, float mu_l, float arrayCoord) {
    IRRADIANCE_COORD_TYPE size = textureSize(irradiance_texture, 0);
    vec3 uv = vec3(
        GetTextureCoordFromUnitRange(
            mu_l * 0.5 + 0.5, size.x
        ),
        GetTextureCoordFromUnitRange(
            (r - planet.surfaceRadius) / (planet.atmosphereThickness), size.y
        ),
        arrayCoord
    );

    return vec3(texture(irradiance_texture, IRRADIANCE_COORD_TYPE(uv)));
}

float DistanceToTopAtmosphereBoundary(const Planet planet,
    float r, float mu) {
  float  discriminant = r * r * (mu * mu - 1.0) +
      planet.atmosphereRadius * planet.atmosphereRadius;
  return ClampDistance(-r * mu + SafeSqrt(discriminant));
}

vec2 GetTransmittanceTextureUvFromRMu(const Planet planet,
sampler2D transm_table,
    float r, float mu) {
  // Distance to top atmosphere boundary for a horizontal ray at ground level.
  float H = sqrt(planet.atmosphereRadius * planet.atmosphereRadius -
      planet.surfaceRadius * planet.surfaceRadius);
  // Distance to the horizon.
  float rho =
      SafeSqrt(r * r - planet.surfaceRadius * planet.surfaceRadius);
  // Distance to the top atmosphere boundary for the ray (r,mu), and its minimum
  // and maximum values over all mu - obtained for (r,1) and (r,mu_horizon).
  float d = DistanceToTopAtmosphereBoundary(planet, r, mu);
  float d_min = planet.atmosphereRadius - r;
  float d_max = rho + H;
  float x_mu = (d - d_min) / (d_max - d_min);
  float x_r = rho / H;
  return vec2(GetTextureCoordFromUnitRange(x_mu, textureSize(transm_table,0).x),
              GetTextureCoordFromUnitRange(x_r, textureSize(transm_table,0).y));
}

/*
<p>and the inverse mapping follows immediately:
*/

void GetRMuFromTransmittanceTextureUv(const Planet planet, vec2 texSize,
    vec2 uv, out float r, out float mu) {
  float x_mu = GetUnitRangeFromTextureCoord(uv.x, texSize.x);
  float x_r = GetUnitRangeFromTextureCoord(uv.y, texSize.y);
  // Distance to top atmosphere boundary for a horizontal ray at ground level.
  float H = sqrt(planet.atmosphereRadius * planet.atmosphereRadius -
      planet.surfaceRadius * planet.surfaceRadius);
  // Distance to the horizon, from which we can compute r:
  float rho = H * x_r;
  r = sqrt(rho * rho + planet.surfaceRadius * planet.surfaceRadius);
  // Distance to the top atmosphere boundary for the ray (r,mu), and its minimum
  // and maximum values over all mu - obtained for (r,1) and (r,mu_horizon) -
  // from which we can recover mu:
  float d_min = planet.atmosphereRadius - r;
  float d_max = rho + H;
  float d = d_min + x_mu * (d_max - d_min);
  mu = d == 0.0 ? float(1.0) : (H * H - rho * rho - d * d) / (2.0 * r * d);
  mu = ClampCosine(mu);
}

vec3 GetTransmittanceToTopAtmosphereBoundary(
    const Planet planet,
    sampler2D transmittance_texture,
    float r, float mu) {
  vec2 uv = GetTransmittanceTextureUvFromRMu(planet, transmittance_texture, r, mu);
  return vec3(texture(transmittance_texture, uv));
}

vec3 GetTransmittanceToLight(
    const Planet planet,
    float angularRadius,
    sampler2D transmittance_texture,
    float r, float mu_l) {
    float sin_theta_h = planet.surfaceRadius/ r;
    float cos_theta_h = -sqrt(max(1.0 - sin_theta_h * sin_theta_h, 0.0));

    return GetTransmittanceToTopAtmosphereBoundary(planet, transmittance_texture, r, mu_l) *
        smoothstep(-sin_theta_h * angularRadius,
            sin_theta_h * angularRadius,
            mu_l - cos_theta_h);
}

vec3 GetTransmittance(
    Planet planet,
    sampler2D transmittance_texture,
    float r, float mu, float d, bool ray_r_mu_intersects_ground) {

  float r_d = ClampRadius(planet, sqrt(d * d + 2.0 * r * mu * d + r * r));
  float mu_d = ClampCosine((r * mu + d) / r_d);

  if (ray_r_mu_intersects_ground) {
    return min(
        GetTransmittanceToTopAtmosphereBoundary(
            planet, transmittance_texture, r_d, -mu_d) /
        GetTransmittanceToTopAtmosphereBoundary(
            planet, transmittance_texture, r, -mu),
        vec3(1.0));
  } else {
    return min(
        GetTransmittanceToTopAtmosphereBoundary(
            planet, transmittance_texture, r, mu) /
        GetTransmittanceToTopAtmosphereBoundary(
            planet, transmittance_texture, r_d, mu_d),
        vec3(1.0));
  }
}

float DistanceToBottomAtmosphereBoundary(Planet planet,
    float r, float mu) {
  float discriminant = r * r * (mu * mu - 1.0) +
      planet.surfaceRadius * planet.surfaceRadius;
  return ClampDistance(-r * mu - SafeSqrt(discriminant));
}

float DistanceToNearestAtmosphereBoundary(Planet planet,
    float r, float mu, bool ray_r_mu_intersects_ground) {
  if (ray_r_mu_intersects_ground) {
    return DistanceToBottomAtmosphereBoundary(planet, r, mu);
  } else {
    return DistanceToTopAtmosphereBoundary(planet, r, mu);
  }
}

void GetRMuMuSNuFromScatteringTextureUvwz(Planet planet,
    vec4 uvwz, out float r, out float mu, out float mu_s,
    out float nu, out bool ray_r_mu_intersects_ground) {

  // Distance to top atmosphere boundary for a horizontal ray at ground level.
  float H = sqrt(planet.atmosphereRadius * planet.atmosphereRadius -
      planet.surfaceRadius * planet.surfaceRadius);
  // Distance to the horizon.
  float rho =
      H * GetUnitRangeFromTextureCoord(uvwz.w, SCATTERING_TEXTURE_R_SIZE);
  r = sqrt(rho * rho + planet.surfaceRadius * planet.surfaceRadius);

  if (uvwz.z < 0.5) {
    // Distance to the ground for the ray (r,mu), and its minimum and maximum
    // values over all mu - obtained for (r,-1) and (r,mu_horizon) - from which
    // we can recover mu:
    float d_min = r - planet.surfaceRadius;
    float d_max = rho;
    float d = d_min + (d_max - d_min) * GetUnitRangeFromTextureCoord(
        1.0 - 2.0 * uvwz.z, SCATTERING_TEXTURE_MU_SIZE / 2);
    mu = d == 0.0 ? 1.0 :
        ClampCosine(-(rho * rho + d * d) / (2.0 * r * d));
    ray_r_mu_intersects_ground = true;
  } else {
    // Distance to the top atmosphere boundary for the ray (r,mu), and its
    // minimum and maximum values over all mu - obtained for (r,1) and
    // (r,mu_horizon) - from which we can recover mu:
    float d_min = planet.atmosphereRadius - r;
    float d_max = rho + H;
    float d = d_min + (d_max - d_min) * GetUnitRangeFromTextureCoord(
        2.0 * uvwz.z - 1.0, SCATTERING_TEXTURE_MU_SIZE / 2);
    mu = d == 0.0 ? 1.0 :
        ClampCosine((H * H - rho * rho - d * d) / (2.0 * r * d));
    ray_r_mu_intersects_ground = false;
  }

  float x_mu_s =
      GetUnitRangeFromTextureCoord(uvwz.y, SCATTERING_TEXTURE_MU_S_SIZE);
  float d_min = planet.atmosphereRadius - planet.surfaceRadius;
  float d_max = H;
  float D = DistanceToTopAtmosphereBoundary(
      planet, planet.surfaceRadius, planet.mu_s_min);
  float A = (D - d_min) / (d_max - d_min);
  float a = (A - x_mu_s * A) / (1.0 + x_mu_s * A);
  float d = d_min + min(a, A) * (d_max - d_min);
  mu_s = d == 0.0 ? float(1.0) :
     ClampCosine((H * H - d * d) / (2.0 * planet.surfaceRadius * d));

  nu = ClampCosine(uvwz.x * 2.0 - 1.0);
}

bool RayIntersectsGround(Planet planet,
    float r, float mu) {
  return mu < 0.0 && r * r * (mu * mu - 1.0) +
      planet.surfaceRadius * planet.surfaceRadius >= 0.0;
}

vec4 GetScatteringTextureUvwzFromRMuMuSNu(Planet planet,
    float r, float mu, float mu_s, float nu,
    bool ray_r_mu_intersects_ground, float lightIndex) {

  // Distance to top atmosphere boundary for a horizontal ray at ground level.
  float H = sqrt(planet.atmosphereRadius * planet.atmosphereRadius -
      planet.surfaceRadius * planet.surfaceRadius);
  // Distance to the horizon.
  float rho =
      SafeSqrt(r * r - planet.surfaceRadius * planet.surfaceRadius);
  float u_r = GetTextureCoordFromUnitRange(rho / H, SCATTERING_TEXTURE_R_SIZE);

  // Discriminant of the quadratic equation for the intersections of the ray
  // (r,mu) with the ground (see RayIntersectsGround).
  float r_mu = r * mu;
  float discriminant =
      r_mu * r_mu - r * r + planet.surfaceRadius * planet.surfaceRadius;
  float u_mu;
  if (ray_r_mu_intersects_ground) {
    // Distance to the ground for the ray (r,mu), and its minimum and maximum
    // values over all mu - obtained for (r,-1) and (r,mu_horizon).
    float d = -r_mu - SafeSqrt(discriminant);
    float d_min = r - planet.surfaceRadius;
    float d_max = rho;
    u_mu = 0.5 - 0.5 * GetTextureCoordFromUnitRange(d_max == d_min ? 0.0 :
        (d - d_min) / (d_max - d_min), SCATTERING_TEXTURE_MU_SIZE / 2);
  } else {
    // Distance to the top atmosphere boundary for the ray (r,mu), and its
    // minimum and maximum values over all mu - obtained for (r,1) and
    // (r,mu_horizon).
    float d = -r_mu + SafeSqrt(discriminant + H * H);
    float d_min = planet.atmosphereRadius - r;
    float d_max = rho + H;
    u_mu = 0.5 + 0.5 * GetTextureCoordFromUnitRange(
        (d - d_min) / (d_max - d_min), SCATTERING_TEXTURE_MU_SIZE / 2);
  }

  float d = DistanceToTopAtmosphereBoundary(
      planet, planet.surfaceRadius, mu_s);
  float d_min = planet.atmosphereRadius - planet.surfaceRadius;
  float d_max = H;
  float a = (d - d_min) / (d_max - d_min);
  float D = DistanceToTopAtmosphereBoundary(
      planet, planet.surfaceRadius, planet.mu_s_min);
  float A = (D - d_min) / (d_max - d_min);
  // An ad-hoc function equal to 0 for mu_s = mu_s_min (because then d = D and
  // thus a = A), equal to 1 for mu_s = 1 (because then d = d_min and thus
  // a = 0), and with a large slope around mu_s = 0, to get more texture 
  // samples near the horizon.
  float u_mu_s = GetTextureCoordFromUnitRange(
      max(1.0 - a / A, 0.0) / (1.0 + a), SCATTERING_TEXTURE_MU_S_SIZE);

  float u_nu = (nu + 1.0) / 2.0;

  // Offset the Y coordinate according to the number of lights in the texture
  u_mu = (u_mu + lightIndex)/SCATTERING_LIGHT_COUNT;
  return vec4(u_nu, u_mu_s, u_mu, u_r);
}

vec3 GetExtrapolatedSingleMieScattering(
    Planet planet, vec4 scattering) {
  // Algebraically this can never be negative, but rounding errors can produce
  // that effect for sufficiently short view rays.
  if (scattering.r <= 0.0) {
    return vec3(0.0);
  }
  return scattering.rgb * scattering.a / scattering.r *
	    (planet.rayleighCoefficients.r / planet.mieCoefficient) *
	    (planet.mieCoefficient / planet.rayleighCoefficients);
}

vec3 GetCombinedScattering(
    Planet planet,
    sampler3D scattering_texture,
    float r, float mu, float mu_s, float nu,
    bool ray_r_mu_intersects_ground,
    float lightIndex,
    out vec3 single_mie_scattering) {
  vec4 uvwz = GetScatteringTextureUvwzFromRMuMuSNu(
      planet, r, mu, mu_s, nu, ray_r_mu_intersects_ground, lightIndex);
  float tex_coord_x = uvwz.x * float(SCATTERING_TEXTURE_NU_SIZE - 1);
  float tex_x = floor(tex_coord_x);
  float lerp = tex_coord_x - tex_x;
  vec3 uvw0 = vec3((tex_x + uvwz.y) / float(SCATTERING_TEXTURE_NU_SIZE),
      uvwz.z, uvwz.w);
  vec3 uvw1 = vec3((tex_x + 1.0 + uvwz.y) / float(SCATTERING_TEXTURE_NU_SIZE),
      uvwz.z, uvwz.w);
  vec4 combined_scattering =
      texture(scattering_texture, uvw0) * (1.0 - lerp) +
      texture(scattering_texture, uvw1) * lerp;
  vec3 scattering = vec3(combined_scattering);
  single_mie_scattering =
      GetExtrapolatedSingleMieScattering(planet, combined_scattering);
  return scattering;
}

float RayleighPhaseFunction(float nu) {
  float k = 3.0 / (16.0 * pi);
  return k * (1.0 + nu * nu);
}

float MiePhaseFunction(float g, float nu) {
  float k = 3.0 / (8.0 * pi) * (1.0 - g * g) / (2.0 + g * g);
  return k * (1.0 + nu * nu) / pow(1.0 + g * g - 2.0 * g * nu, 1.5);
}


vec3 GetSkyRadianceToPoint(
    Planet planet,
    sampler2D transmittance_texture,
    sampler3D scattering_texture,
    vec3 camera, float d, vec3 view_ray, float shadow_length,
    vec3 sun_direction, float lightIndex, out vec3 transmittance) {
  // Compute the distance to the top atmosphere boundary along the view ray,
  // assuming the viewer is in space (or NaN if the view ray does not intersect
  // the atmosphere).
  float r = length(camera);
  float rmu = dot(camera, view_ray);
  float distance_to_top_atmosphere_boundary = -rmu -
      sqrt(rmu * rmu - r * r + planet.atmosphereRadius * planet.atmosphereRadius);
  // If the viewer is in space and the view ray intersects the planet, move
  // the viewer to the top atmosphere boundary (along the view ray):
  if (distance_to_top_atmosphere_boundary > 0.0) {
    camera = camera + view_ray * distance_to_top_atmosphere_boundary;
    r = planet.atmosphereRadius;
    rmu += distance_to_top_atmosphere_boundary;
  }

  // Compute the r, mu, mu_s and nu parameters for the first texture lookup.
  float mu = rmu / r;
  float mu_s = dot(camera, sun_direction) / r;
  float nu = dot(view_ray, sun_direction);
  bool ray_r_mu_intersects_ground = RayIntersectsGround(planet, r, mu);

  transmittance = GetTransmittance(planet, transmittance_texture,
      r, mu, d, ray_r_mu_intersects_ground);

  vec3 single_mie_scattering;
  vec3 scattering = GetCombinedScattering(
      planet, scattering_texture,
      r, mu, mu_s, nu, ray_r_mu_intersects_ground, lightIndex,
      single_mie_scattering);

  // Compute the r, mu, mu_s and nu parameters for the second texture lookup.
  // If shadow_length is not 0 (case of light shafts), we want to ignore the
  // scattering along the last shadow_length meters of the view ray, which we
  // do by subtracting shadow_length from d (this way scattering_p is equal to
  // the S|x_s=x_0-lv term in Eq. (17) of our paper).
  d = max(d - shadow_length, 0.0);
  float r_p = ClampRadius(planet, sqrt(d * d + 2.0 * r * mu * d + r * r));
  float mu_p = (r * mu + d) / r_p;
  float mu_s_p = (r * mu_s + d * nu) / r_p;

  vec3 single_mie_scattering_p;
  vec3 scattering_p = GetCombinedScattering(
      planet, scattering_texture,
      r_p, mu_p, mu_s_p, nu, ray_r_mu_intersects_ground, lightIndex,
      single_mie_scattering_p);

  // Combine the lookup results to get the scattering between camera and point.
  vec3 shadow_transmittance = transmittance;
  if (shadow_length > 0.0) {
    // This is the T(x,x_s) term in Eq. (17) of our paper, for light shafts.
    shadow_transmittance = GetTransmittance(planet, transmittance_texture,
        r, mu, d, ray_r_mu_intersects_ground);
  }
  scattering = scattering - shadow_transmittance * scattering_p;
  single_mie_scattering =
      single_mie_scattering - shadow_transmittance * single_mie_scattering_p;
  single_mie_scattering = GetExtrapolatedSingleMieScattering(
      planet, vec4(scattering, single_mie_scattering.r));

  // Hack to avoid rendering artifacts when the sun is below the horizon.
  single_mie_scattering = single_mie_scattering *
      smoothstep(float(0.0), float(0.01), mu_s);

  return scattering * RayleighPhaseFunction(nu) + single_mie_scattering *
      MiePhaseFunction(planet.mieAsymmetryFactor, nu);
}

vec2 GetIrradianceTextureUvFromRMuS(Planet planet,
    float r, float mu_s, vec2 textureDimensions) {
  float x_r = (r - planet.surfaceRadius) /
      (planet.atmosphereRadius - planet.surfaceRadius);
  float x_mu_s = mu_s * 0.5 + 0.5;
  return vec2(GetTextureCoordFromUnitRange(x_mu_s, textureDimensions.x),
              GetTextureCoordFromUnitRange(x_r, textureDimensions.y));
}


void GetRMuSFromIrradianceTextureUv(Planet planet,
    vec2 uv, out float r, out float mu_s, vec2 textureDimensions) {
  float x_mu_s = GetUnitRangeFromTextureCoord(uv.x, textureDimensions.x);
  float x_r = GetUnitRangeFromTextureCoord(uv.y, textureDimensions.y);
  r = planet.surfaceRadius +
      x_r * (planet.atmosphereRadius - planet.surfaceRadius);
  mu_s = ClampCosine(2.0 * x_mu_s - 1.0);
}

// Get relative ozone density by sample height - multipied by segmentLength
float ozoneHF(float sampleHeight, Planet planet, float segmentLength)
{
    float result;
    if(sampleHeight < planet.ozonePeakHeight)
    {
        result = planet.ozoneTroposphereCoef * sampleHeight + planet.ozoneTroposphereConst;
    }
    else
    {
        result = planet.ozoneStratosphereCoef * sampleHeight + planet.ozoneStratosphereConst;
    }
    return clamp(result, 0, 1) * segmentLength;
}
#endif