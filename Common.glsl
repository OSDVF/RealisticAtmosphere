//?#version 440
#include "Structures.glsl"

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
    sampler2D irradiance_texture,
    float r, float mu_s) {
    vec2 size = textureSize(irradiance_texture, 0);
    vec2 uv = vec2(
        GetTextureCoordFromUnitRange(
            mu_s * 0.5 + 0.5, size.x
        ),
        GetTextureCoordFromUnitRange(
            (r - planet.surfaceRadius) / (planet.atmosphereThickness), size.y
        )
    );

    return vec3(texture(irradiance_texture, uv));
}

vec3 GetTransmittanceToAtmosphereEnd(
    const Planet planet,
    sampler2D transmittance_texture,
    float r, float mu) {
    float atmoRadius2 = planet.atmosphereRadius * planet.atmosphereRadius;
    float bottomRadius2 = planet.surfaceRadius * planet.surfaceRadius;
    float H = sqrt(atmoRadius2 - bottomRadius2);
    float rho = sqrt(max(r * r - bottomRadius2, 0.0));
    float d = max(-r * mu + sqrt(max((mu * mu - 1.0) * r * r + atmoRadius2, 0.0)), 0.0);

    float d_min = planet.atmosphereRadius - r;
    float d_max = rho + H;
    float x_mu = (d - d_min) / (d_max - d_min);
    float x_r = rho / H;
    vec2 size = textureSize(transmittance_texture,0);
    vec2 uv = vec2(
        GetTextureCoordFromUnitRange(x_mu, size.x),
        GetTextureCoordFromUnitRange(x_r, size.y));

    return vec3(texture(transmittance_texture, uv));
}

vec3 GetTransmittanceToSun(
    const Planet planet,
    sampler2D transmittance_texture,
    float r, float mu_s) {
    float sin_theta_h = planet.surfaceRadius/ r;
    float cos_theta_h = -sqrt(max(1.0 - sin_theta_h * sin_theta_h, 0.0));

    return GetTransmittanceToAtmosphereEnd(planet, transmittance_texture, r, mu_s) *
        smoothstep(-sin_theta_h * planet.sunAngularRadius,
            sin_theta_h * planet.sunAngularRadius,
            mu_s - cos_theta_h);
}