//?#version 440
#ifndef LAMBERT_H
#define LAMBERT_H
// https://github.com/knightcrawler25/GLSL-PathTracer/blob/master/src/shaders/common/lambert.glsl
#include "Sampling.glsl"
/*
 * MIT License
 *
 * Copyright(c) 2019 Asif Ali
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

 vec3 LambertSample(vec3 albedo, vec3 V, vec3 N, inout vec3 L, inout float pdf)
{
    float r1 = random(time*V.x);
    float r2 = random(time*V.y);

    vec3 T, B;
    Onb(N, T, B);

    L = CosineSampleHemisphere(r1, r2);
    L = T * L.x + B * L.y + N * L.z;

    pdf = dot(N, L) * (1.0 / PI);

    return pdf * albedo;
}

vec3 LambertEval(vec3 albedo, vec3 V, vec3 N, vec3 L, inout float pdf)
{
    pdf = dot(N, L) * INV_PI;
    return pdf * albedo;
}
#endif