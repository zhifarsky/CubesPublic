#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform float scale;
uniform int spherical; // 1 for spherical; 0 for cylindrical

void main()
{
    mat4 billboardView = view * model;
    billboardView[0][0] = scale;
    billboardView[0][1] = 0.0;
    billboardView[0][2] = 0.0;

    if (spherical == 1) {
        billboardView[1][0] = 0.0;
        billboardView[1][2] = 0.0;
    }
    billboardView[1][1] = scale;

    billboardView[2][0] = 0.0;
    billboardView[2][1] = 0.0;
    billboardView[2][2] = scale;

    vec4 P = billboardView * vec4(aPos, 1.0);
	gl_Position = projection * P;
	TexCoord = aTexCoord;
}