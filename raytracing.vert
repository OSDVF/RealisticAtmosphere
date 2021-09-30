in vec3 a_position;
in vec3 a_color0;
out vec3 v_color0;

uniform mat4 u_modelViewProj;

void main()
{
	gl_Position = u_modelViewProj * vec4(a_position, 1.0);
	v_color0 = a_color0;
}
