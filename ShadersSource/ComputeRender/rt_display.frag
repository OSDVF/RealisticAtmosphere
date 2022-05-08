//!#version 440
//!#define BGFX_SHADER_LANGUAGE_GLSL
// This shader just displays output from raytracer in Compute shader
in vec2 texCoord;
uniform sampler2D colorOutput;
uniform sampler2D normalsBuffer;

layout(location = 0) out vec4 fragColor;

#include "../PostProcess.glsl"

void main()
{
    if(DEBUG_NORMALS)
    {
        fragColor.rgb = texelFetch(normalsBuffer, ivec2(texCoord * textureSize(normalsBuffer, 0)), 0).xyz * 0.5 + 0.5;
    }
    else
    {
		fragColor.rgb = texelFetch(colorOutput, ivec2(texCoord * textureSize(colorOutput, 0)), 0).xyz;

        if(HQSettings.w != 0)//Zero indicates no post-processing
        {
		    fragColor.rgb = postProcess(fragColor.rgb, texCoord);
        }
    }
}