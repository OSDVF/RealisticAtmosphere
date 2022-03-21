#include "ColorMapping.h"
#include <cmath>
#include <cassert>
#include <array>

namespace ColorMapping {

    double CieColorMatchingFunctionTableValue(double wavelength, int column) {
        if (wavelength <= kLambdaMin || wavelength >= kLambdaMax) {
            return 0.0;
        }
        double u = (wavelength - kLambdaMin) / 5.0;
        int row = static_cast<int>(std::floor(u));
        assert(row >= 0 && row + 1 < 95);
        assert(CIE_2_DEG_COLOR_MATCHING_FUNCTIONS[4 * row] <= wavelength &&
            CIE_2_DEG_COLOR_MATCHING_FUNCTIONS[4 * (row + 1)] >= wavelength);
        u -= row;
        return CIE_2_DEG_COLOR_MATCHING_FUNCTIONS[4 * row + column] * (1.0 - u) +
            CIE_2_DEG_COLOR_MATCHING_FUNCTIONS[4 * (row + 1) + column] * u;
    }

    double Interpolate(
        const std::vector<double>& wavelengths,
        const std::vector<double>& wavelength_function,
        double wavelength) {
        assert(wavelength_function.size() == wavelengths.size());
        if (wavelength < wavelengths[0]) {
            return wavelength_function[0];
        }
        for (unsigned int i = 0; i < wavelengths.size() - 1; ++i) {
            if (wavelength < wavelengths[i + 1]) {
                double u =
                    (wavelength - wavelengths[i]) / (wavelengths[i + 1] - wavelengths[i]);
                return
                    wavelength_function[i] * (1.0 - u) + wavelength_function[i + 1] * u;
            }
        }
        return wavelength_function[wavelength_function.size() - 1];
    }

    vec3 GetValuesForRGBSpectrum(const std::vector<double>& wavelengths, const std::vector<double>& solar_irradiance)
    {
        return {
            (float)Interpolate(wavelengths, solar_irradiance, kLambdaR),
            (float)Interpolate(wavelengths, solar_irradiance, kLambdaG),
            (float)Interpolate(wavelengths, solar_irradiance, kLambdaB)
        };
    }

    /*
    <p>We can then implement a utility function to compute the "spectral radiance to
    luminance" conversion constants (see Section 14.3 in <a
    href="https://arxiv.org/pdf/1612.04336.pdf">A Qualitative and Quantitative
    Evaluation of 8 Clear Sky Models</a> for their definitions):
    */

    // The returned constants are in lumen.nm / watt.
    void ComputeSpectralRadianceToLuminanceFactors(
        const std::vector<double>& wavelengths,
        const std::vector<double>& solar_irradiance,
        double lambda_power, double* k_r, double* k_g, double* k_b) {
        *k_r = 0.0;
        *k_g = 0.0;
        *k_b = 0.0;
        double solar_r = Interpolate(wavelengths, solar_irradiance, kLambdaR);
        double solar_g = Interpolate(wavelengths, solar_irradiance, kLambdaG);
        double solar_b = Interpolate(wavelengths, solar_irradiance, kLambdaB);
        int dlambda = 1;
        for (int lambda = kLambdaMin; lambda < kLambdaMax; lambda += dlambda) {
            double x_bar = CieColorMatchingFunctionTableValue(lambda, 1);
            double y_bar = CieColorMatchingFunctionTableValue(lambda, 2);
            double z_bar = CieColorMatchingFunctionTableValue(lambda, 3);
            const double* xyz2srgb = XYZ_TO_SRGB;
            double r_bar =
                xyz2srgb[0] * x_bar + xyz2srgb[1] * y_bar + xyz2srgb[2] * z_bar;
            double g_bar =
                xyz2srgb[3] * x_bar + xyz2srgb[4] * y_bar + xyz2srgb[5] * z_bar;
            double b_bar =
                xyz2srgb[6] * x_bar + xyz2srgb[7] * y_bar + xyz2srgb[8] * z_bar;
            double irradiance = Interpolate(wavelengths, solar_irradiance, lambda);
            *k_r += r_bar * irradiance / solar_r *
                pow(lambda / kLambdaR, lambda_power);
            *k_g += g_bar * irradiance / solar_g *
                pow(lambda / kLambdaG, lambda_power);
            *k_b += b_bar * irradiance / solar_b *
                pow(lambda / kLambdaB, lambda_power);
        }
        *k_r *= dlambda * MAX_LUMINOUS_EFFICACY;
        *k_g *= dlambda * MAX_LUMINOUS_EFFICACY;
        *k_b *= dlambda * MAX_LUMINOUS_EFFICACY;
    }

    void FillSpectrum(vec4& SkyRadianceToLuminance, vec4& SunRadianceToLuminance, Planet& planet, DirectionalLight& sun)
    {
        std::vector<double> wavelengths;
        std::vector<double> solar_irradiance;
        std::vector<double> rayleigh_scattering;
        std::vector<double> mie_scattering;
        std::vector<double> mie_extinction;
        std::vector<double> absorption_extinction;
        std::vector<double> ground_albedo;

        for (int l = kLambdaMin; l <= kLambdaMax; l += 10) {
            double lambda = static_cast<double>(l) * 1e-3;  // micro-meters
            double mie =
                kMieAngstromBeta / kMieScaleHeight * pow(lambda, -kMieAngstromAlpha);
            wavelengths.push_back(l);
            solar_irradiance.push_back(kSolarIrradiance[(l - kLambdaMin) / 10]);
            rayleigh_scattering.push_back(kRayleigh * pow(lambda, -4));
            mie_scattering.push_back(mie * kMieSingleScatteringAlbedo);
            mie_extinction.push_back(mie);
            absorption_extinction.push_back(kMaxOzoneNumberDensity * kOzoneCrossSection[(l - kLambdaMin) / 10]);
            ground_albedo.push_back(kGroundAlbedo);
        }


        // Compute the values for the SKY_RADIANCE_TO_LUMINANCE constant
        double sky_k_r, sky_k_g, sky_k_b;
        ComputeSpectralRadianceToLuminanceFactors(wavelengths, solar_irradiance,
            -3 /* lambda_power */, &sky_k_r, &sky_k_g, &sky_k_b);
        SkyRadianceToLuminance.x = sky_k_r;
        SkyRadianceToLuminance.y = sky_k_g;
        SkyRadianceToLuminance.z = sky_k_b;
        // Compute the values for the SUN_RADIANCE_TO_LUMINANCE constant.
        double sun_k_r, sun_k_g, sun_k_b;
        ComputeSpectralRadianceToLuminanceFactors(wavelengths, solar_irradiance,
            0 /* lambda_power */, &sun_k_r, &sun_k_g, &sun_k_b);
        SunRadianceToLuminance.x = sun_k_r;
        SunRadianceToLuminance.y = sun_k_g;
        SunRadianceToLuminance.z = sun_k_b;

        sun.irradiance = GetValuesForRGBSpectrum(wavelengths, solar_irradiance);
        planet.absorptionCoefficients = GetValuesForRGBSpectrum(wavelengths, absorption_extinction);
        planet.rayleighCoefficients = GetValuesForRGBSpectrum(wavelengths, rayleigh_scattering);
        vec3 mie = GetValuesForRGBSpectrum(wavelengths, mie_scattering);
        planet.mieCoefficient = (mie.x + mie.y + mie.z) / 3.0;
    }
};