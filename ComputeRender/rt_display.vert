#version 440
in vec3 a_position;
in vec2 a_texCoord;

out vec2 texCoord;
void main()
{
    texCoord = a_texCoord;
}