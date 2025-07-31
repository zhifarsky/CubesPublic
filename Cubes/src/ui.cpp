#include "ui.h"
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>
#include "ResourceLoader.h"
#include "Mesh.h"
#include "Directories.h"

static GLuint uiShader;
static GLuint faceVAO;

float originX = 0, originY = 0;
float left, right, bottom, top;
int displayW, displayH;

glm::mat4 projection;

void uiInit() {
	uiShader = BuildShader(SHADER_FOLDER "uiElement.vert", SHADER_FOLDER "uiElement.frag");

	// TODO: запечь вершины в вершинный шейдер вместо создани€ их на CPU
	static Vertex faceVerts[] = {
		Vertex(-0.5,-0.5,0, 0,0),
		Vertex(0.5, -0.5,0, 1,0),
		Vertex(0.5, 0.5,0, 1,1),
		Vertex(-0.5,0.5,0, 0,1),
	};
	static Triangle faceTris[] = {
		Triangle(0,1,2),
		Triangle(0,2,3)
	};

	GLuint VAO, VBO, EBO;
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &EBO);

	// выбираем буфер как текущий
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);

	glBufferData(GL_ARRAY_BUFFER, sizeof(faceVerts), faceVerts, GL_STATIC_DRAW); // отправка вершин в пам€ть видеокарты
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(faceTris), faceTris, GL_STATIC_DRAW); // загружаем индексы

	// "объ€сн€ем" как необходимо прочитать массив с вершинами
	GLint stride = sizeof(Vertex);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, pos)); // pos
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, uv)); // tex coord

	faceVAO = VAO;
	glBindVertexArray(0);
}

void uiStart(GLFWwindow* window) {
	glfwGetFramebufferSize(window, &displayW, &displayH);

	originX = originY = 0;

	projection = glm::mat4(1.0);
	float aspect = (float)displayH / (float)displayW;
	float centerX = (float)displayW / 2.0f;
	float centerY = (float)displayH / 2.0f;
	left = -centerX;
	right = centerX;
	bottom = -centerY;
	top = centerY;
	//projectionOrtho = glm::ortho(0.0f, (float)display_w, 0.0f, (float)display_h, -1.0f, 100.0f);
	//projectionOrtho = glm::ortho(-1.0f, 1.0f, -1.0f * aspect, 1.0f * aspect, -1.0f, 1.0f);
	projection = glm::ortho(-centerX, centerX, -centerY, centerY, -1.0f, 1.0f);

	// use ui shader
	glUseProgram(uiShader);
	glUniformMatrix4fv(glGetUniformLocation(uiShader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
}


void uiDrawElement(GLuint texture, glm::vec3 rot, glm::vec3 scale, glm::vec2 uvScale, glm::vec2 uvShift) {
	glBindTexture(GL_TEXTURE_2D, texture);

	glm::mat4 model = glm::mat4(1.0f); // единична€ матрица (1 по диагонали)
	model = glm::translate(model, glm::vec3(originX, originY, 0.0f));
	model = glm::rotate(model, glm::radians(rot.x), glm::vec3(1.0, 0.0, 0.0));
	model = glm::rotate(model, glm::radians(rot.y), glm::vec3(0.0, 1.0, 0.0));
	model = glm::rotate(model, glm::radians(rot.z+180), glm::vec3(0.0, 0.0, 1.0)); // +180 так как элемент отрисовывалс€ вверх ногами
	model = glm::scale(model, scale);
	glUniformMatrix4fv(glGetUniformLocation(uiShader, "model"), 1, GL_FALSE, glm::value_ptr(model));

	glUniform2f(glGetUniformLocation(uiShader, "UVScale"), uvScale.x, uvScale.y);
	glUniform2f(glGetUniformLocation(uiShader, "UVShift"), uvShift.x, uvShift.y);

	glBindVertexArray(faceVAO);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

void uiSetAnchor(uiAnchor anchor, float offset) {
	switch (anchor) {
	case uiLeftAnchor:
		originX = left + offset;
		break;
	case uiRightAnchor:
		originX = right - offset;
		break;
	case uiTopAnchor:
		originY = top - offset;
		break;
	case uiBottomAnchor:
		originY = bottom + offset;
		break;
	}
}

void uiSetOrigin(float x, float y) {
	originX = x;
	originY = y;
}

void uiShiftOrigin(float offsetX, float offsetY) {
	originX += offsetX;
	originY += offsetY;
}