//!#version 440
#include "raytracer.glsl"
// This variant runs raytracer inside fragment shader

void main()
{
	gl_FragColor = vec4(raytrace(vec2(gl_FragCoord.x, u_viewRect.w - gl_FragCoord.y)),1.0);
}