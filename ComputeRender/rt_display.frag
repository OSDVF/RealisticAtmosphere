//!#version 440
//!#define BGFX_SHADER_LANGUAGE_GLSL
// This shader just displays output from raytracer in Compute shader
in vec2 texCoord;
uniform sampler2D colorOutput;
uniform sampler2D normalsBuffer;
uniform sampler2D depthBuffer;
uniform vec4 DisplaySettings;
layout(location = 0) out vec4 fragColor;
uniform vec4 u_viewRect;
uniform vec4 HQSettings;
#define SunScreenPos (DisplaySettings.zw)
#define TonemappingType floatBitsToInt(DisplaySettings.x)
#define FlareBrightness DisplaySettings.y
#define HQSettings_exposure HQSettings.w
uniform vec4 MultisamplingSettings;
#include "../Debug.glsl"
#include "../Tonemapping.h"
#include "../Random.glsl"

const float flareSmallness0 = 100;
const float flareSize1 = 0;
const float flareBrightness0 = 0.6;
const float flareBrightness2 = 0.2;

vec3 lensflare(vec2 uv, vec2 pos, float occlusion)
{
	vec2 main = uv-pos;
	float centerness = length(uv);
	vec2 uvd = uv*(length(uv));
	
	float ang = atan(main.x,main.y);
	float dist=length(main); dist = pow(dist,.1);
	
	float f0 = 1.0/(length(uv-pos)*flareSmallness0+1.0);
	
	f0 = f0 + f0*(sin(2*noise(vec2(sin(ang*2.+pos.x)*4.0 - cos(ang*3.+pos.y),0) - 1)*16.)*.1 + dist*flareSize1);

	f0 *=flareBrightness0 * clamp((1-occlusion),0,1);
	
	float f1 = max(0.01-pow(length(uv+1.2*pos),1.9),.0)*7.0;

	float f2 = max(1.0/(1.0+32.0*pow(length(uvd+0.8*pos),2)),.0)*00.25 * centerness;
	float f22 = max(1.0/(1.0+32.0*pow(length(uvd+0.85*pos),2)),.0)*00.23 * centerness;
	float f23 = max(1.0/(1.0+32.0*pow(length(uvd+0.9*pos),2)),.0)*00.21 * centerness;
	
	vec2 uvx = mix(uv,uvd,-0.5);
	
	float f4 = max(0.01-pow(length(uvx+0.4*pos),2.4),.0)*6.0;
	float f42 = max(0.01-pow(length(uvx+0.45*pos),2.4),.0)*5.0;
	float f43 = max(0.01-pow(length(uvx+0.5*pos),2.4),.0)*3.0;
	
	uvx = mix(uv,uvd,-.4);
	
	float f5 = max(0.01-pow(length(uvx+0.2*pos),5.5),.0)*2.0;
	float f52 = max(0.01-pow(length(uvx+0.4*pos),5.5),.0)*2.0;
	float f53 = max(0.01-pow(length(uvx+0.6*pos),5.5),.0)*2.0;
	
	uvx = mix(uv,uvd,-0.5);
	
	float f6 = max(0.01-pow(length(uvx-0.3*pos),1.6),.0)*6.0;
	float f62 = max(0.01-pow(length(uvx-0.325*pos),1.6),.0)*3.0;
	float f63 = max(0.01-pow(length(uvx-0.35*pos),1.6),.0)*5.0;
	
	vec3 c = vec3(.0);
	
	c.r+=f2+f4+f5+f6; c.g+=f22+f42+f52+f62; c.b+=f23+f43+f53+f63;
	c = c*1.3 - vec3(length(uvd)*.05);
	c+=vec3(f0);
	
	return c;
}

vec3 tonemapping(vec3 hdrColor)
{
    if(DEBUG_NORMALS || DEBUG_RM)
        return hdrColor;

    return vec3(
		tmFunc(hdrColor.x, TonemappingType),
		tmFunc(hdrColor.y, TonemappingType),
		tmFunc(hdrColor.z, TonemappingType)
		);
}


void main()
{
    if(DEBUG_NORMALS)
    {
        fragColor.xyz = texture2D(normalsBuffer,texCoord).xyz * 0.5 + 0.5;
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
		vec2 aspectRatio = vec2(u_viewRect.z/u_viewRect.w,1);
		fragColor.xyz += FlareBrightness
						* vec3(1.4,1.2,1.0)
						* lensflare(
							(texCoord - 0.5) * aspectRatio,
							SunScreenPos,
							min(texture(depthBuffer, texCoord).x, 1)
						);
    }
}