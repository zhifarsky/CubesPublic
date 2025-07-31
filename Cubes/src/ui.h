#pragma once
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <glm.hpp>

void uiInit();
void uiStart(GLFWwindow* window);

void uiDrawElement(GLuint texture, glm::vec3 rot, glm::vec3 scale, glm::vec2 uvScale, glm::vec2 uvShift);

enum uiAnchor {
	uiLeftAnchor,
	uiRightAnchor,
	uiTopAnchor,
	uiBottomAnchor
};

void uiSetAnchor(uiAnchor anchor, float offset);
void uiShiftOrigin(float offsetX, float offsetY);
void uiSetOrigin(float x, float y);
