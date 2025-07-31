#version 330 core
out vec4 FragColor;

in vec3 ourColor;
in vec2 TexCoord;

//uniform sampler2D texture1;
//uniform float col_mix;
uniform vec4 color;

void main()
{
	FragColor = color;
}