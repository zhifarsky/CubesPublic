#include <iostream>
#include <windows.h>
#include <glad/glad.h>
#include <SOIL/SOIL.h>
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <exception>
#include "ResourceLoader.h"
#include "Tools.h"

void ReadShaderErrors(GLuint shader) {
	int  success;
	char infoLog[512];
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(shader, 512, NULL, infoLog);
		//std::cout << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
		OutputDebugStringA("ERROR::SHADER::COMPILATION_FAILED\n");
		OutputDebugStringA(infoLog);
		OutputDebugStringA("\n");
		FatalError(infoLog);
	}
}

void ReadShaderProgramErrors(GLuint shaderProgram) {
	int  success;
	char infoLog[512];
	glGetProgramiv(shaderProgram, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
		//std::cout << "ERROR::SHADER::PROGRAM::COMPILATION_FAILED\n" << infoLog << std::endl;
		OutputDebugStringA("ERROR::SHADER_PROGRAM::COMPILATION_FAILED\n");
		OutputDebugStringA(infoLog);
		OutputDebugStringA("\n");
		FatalError(infoLog);
	}
}

GLuint BuildShader(const char* vertexShaderFilename, const char* fragmentShaderFilename) {
	std::ifstream vs(vertexShaderFilename);
	std::ifstream fs(fragmentShaderFilename);
	if (!vs.is_open() || !fs.is_open()) {
		vs.close();
		fs.close();
		FatalError("Error on loading shaders from disk");
	}

	std::ostringstream ss1, ss2;
	ss1 << vs.rdbuf();
	ss2 << fs.rdbuf();
	std::string vs_str = ss1.str();
	std::string fs_str = ss2.str();
	const char* vs_cstr = vs_str.c_str();
	const char* fs_cstr = fs_str.c_str();

	vs.close();
	fs.close();

	// создаем Vertex Shader
	unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vs_cstr, NULL);
	glCompileShader(vertexShader);
	ReadShaderProgramErrors(vertexShader);

	// создадим Fragment Shader
	unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fs_cstr, NULL);
	glCompileShader(fragmentShader);
	ReadShaderProgramErrors(fragmentShader);

	// создадим Shader Program (объединим Vertex Shader и Fragment Shader)
	unsigned int shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	glLinkProgram(shaderProgram);
	ReadShaderProgramErrors(shaderProgram);

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
	
	return shaderProgram;
}

Texture LoadTexture(const char* path, TextureType textureType) {
	Texture texture;
	
	int forceChannels;
	switch (textureType) {
		case textureRGBA:
			texture.type = textureRGBA;
			forceChannels = SOIL_LOAD_RGBA; 
			break;
		default:
			FatalError("Unknown texture type");
	}
	
	unsigned char* image = SOIL_load_image(path, &texture.width, &texture.height, 0, forceChannels);
	glGenTextures(1, &texture.ID);
	if (image) {
		glBindTexture(GL_TEXTURE_2D, texture.ID);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture.width, texture.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
		glGenerateMipmap(GL_TEXTURE_2D);

		// texture filtering
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	else {
		FatalError("Error on loading textures from disk");
	}
	SOIL_free_image_data(image);
	
	return texture;
}