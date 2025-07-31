#pragma once
#include <glad/glad.h>

void ReadShaderErrors(GLuint shader);

void ReadShaderProgramErrors(GLuint shaderProgram);

GLuint BuildShader(const char* vertexShaderFilename, const char* fragmentShaderFilename);

enum TextureType : char {
	textureRGBA
};

struct Texture {
	GLuint ID;
	TextureType type;
	int width, height;
};

Texture LoadTexture(const char* path, TextureType textureType);