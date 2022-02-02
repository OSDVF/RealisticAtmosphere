//!#version 440
// This shader just displays output from raytracer in Compute shader
in vec2 texCoord;
layout(binding=0) uniform sampler2D colorOutput;
uniform vec4 HQSettings;
uniform vec4 MultisamplingSettings;
#include "../Debug.glsl"

float tmFunc(float hdrColor)
{
    return hdrColor < 1.4131 ? /*gamma correction*/ pow(hdrColor * 0.38317, 1.0 / 2.2) : 1.0 - exp(-hdrColor)/*exposure tone mapping*/;
}

vec3 tonemapping(vec3 hdrColor)
{
    if(DEBUG_NORMALS || DEBUG_RM)
        return hdrColor;
    return vec3(tmFunc(hdrColor.x),tmFunc(hdrColor.y),tmFunc(hdrColor.z));
}

void main()
{
    if(DEBUG_NORMALS)
    {
        gl_FragColor.xyz = texture2D(colorOutput,texCoord).xyz;
    }
    else
    {
        gl_FragColor.xyz = tonemapping(
            (
                texture2D(colorOutput,texCoord).xyz
            )
            /float((floatBitsToInt(HQSettings.y/*sampleNum*/)+1)/floatBitsToInt(MultisamplingSettings.x)/*indirectSamples*/)
        );
    }
}