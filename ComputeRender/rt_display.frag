//!#version 440
// This shader just displays output from raytracer in Compute shader

in vec2 texCoord;
uniform sampler2D computeShaderOutput;

void main()
{
   gl_FragColor = texture2D(computeShaderOutput,texCoord);
}