//!#version 440
#include "raytracer.glsl"
// This variant runs raytracer inside fragment shader

void main()
{
	gl_FragColor = vec4(raytrace(vec2(gl_FragCoord.x - 0.5, u_viewRect.w - gl_FragCoord.y - 0.5)),1.0);
}