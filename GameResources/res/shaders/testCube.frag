#version 330 core

out vec4 FragColor;

in vec3 ourNormal;
in vec3 FragPos;

uniform vec3 sunDir;

void main() {
	vec3 ambientColor = vec3(0.1,0.1,0.3);
	
	vec3 norm = normalize(ourNormal);
	vec3 lightDir = normalize(sunDir);
	float diff = max(dot(norm, lightDir), 0.0);
	vec3 diffuse = diff * vec3(.7,.2,.2);
	vec3 result = ambientColor + diffuse;
	FragColor = vec4(result, 1.0);
}