//!#version 440
// This shader just displays output from raytracer in Compute shader
in vec2 texCoord;
layout(binding=0) uniform sampler2D colorOutput;
layout(location = 0) out vec4 fragColor;
uniform vec4 HQSettings;
#define HQSettings_exposure HQSettings.w
uniform vec4 MultisamplingSettings;
#include "../Debug.glsl"
#include "../Tonemapping.h"

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
        fragColor.xyz = texture2D(colorOutput,texCoord).xyz;
    }
    else
    {
        fragColor.xyz = tonemapping(
            (
                texture2D(colorOutput,texCoord).xyz
            )
            /
            max
            (
                float
                (
                    (floatBitsToInt(HQSettings.y/*sampleNum*/)+1)
                    /
                    floatBitsToInt(MultisamplingSettings.x)/*indirectSamples*/
                ),
                0
            )
        );
    }
}