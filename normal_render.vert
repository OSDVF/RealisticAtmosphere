//!#version 440
in vec3 a_position;

uniform mat4 u_modelViewProj;

void main()
{
	gl_Position = u_modelViewProj * vec4(a_position, 1.0);
}
