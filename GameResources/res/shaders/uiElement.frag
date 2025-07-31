#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

// texture sampler
uniform sampler2D texture1;
uniform vec2 UVScale;
uniform vec2 UVShift;

void main()
{
	//vec2 ScaledUV = vec2(TexCoord.x * UVScale.x, TexCoord.y * UVScale.y);
	vec4 texColor = texture(texture1, TexCoord * UVScale + UVShift);
	if(texColor.a < 0.1) {
		discard;
	}
	FragColor = texColor;
}