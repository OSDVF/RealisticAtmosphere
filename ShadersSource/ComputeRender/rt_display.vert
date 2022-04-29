//!#version 440
in vec3 a_position;
in vec2 a_texcoord0;

out vec2 texCoord;
uniform mat4 u_modelViewProj;
void main()
{
    texCoord = a_texcoord0;
    gl_Position = u_modelViewProj * vec4(a_position, 1.0);
}