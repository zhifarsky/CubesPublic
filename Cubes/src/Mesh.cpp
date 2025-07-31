#include <glad/glad.h>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>
#include "Mesh.h"
#include "ResourceLoader.h"
#include "Directories.h"


static GLuint 
	cubeInstancedShader = NULL,
	polyMeshShader = NULL,
	flatShader = NULL, 
	spriteShader = NULL;

static GLuint* shaders[] = {
	&cubeInstancedShader,
	&polyMeshShader,
	&flatShader,
	&spriteShader
};

Vertex::Vertex() {}
Vertex::Vertex(float x, float y, float z) {
	pos.x = x;
	pos.y = y;
	pos.z = z;
	uv = { 0,0 };
}
Vertex::Vertex(float x, float y, float z, float u, float v) {
	pos.x = x;
	pos.y = y;
	pos.z = z;
	uv.x = u;
	uv.y = v;
}
Vertex::Vertex(float x, float y, float z, float u, float v, glm::vec3 color, glm::vec3 normal) {
	pos.x = x;
	pos.y = y;
	pos.z = z;
	uv.x = u;
	uv.y = v;
	this->color = color;
	this->normal = normal;
}

Triangle::Triangle() {
}
Triangle::Triangle(int a, int b, int c) {
	indices[0] = a;
	indices[1] = b;
	indices[2] = c;
}

BlockFaceInstance::BlockFaceInstance(int pos, BlockFace face, TextureID textureID) {
	this->pos = pos;
	this->face = face;
	this->textureID = textureID;
}

BlockMesh::BlockMesh() {
	faces = 0;
	faceCount = faceSize = 0;
	VAO = VBO = 0;

}

void initShaders() {
	//for(GLuint* shader : shaders)
	//{
	//	if (*shader != NULL)
	//		glDeleteProgram(*shader);
	//}
	
	cubeInstancedShader = BuildShader(SHADER_FOLDER "block.vert", SHADER_FOLDER "block.frag");
	polyMeshShader = BuildShader(SHADER_FOLDER "polyMesh.vert", SHADER_FOLDER "polyMesh.frag");
	flatShader = BuildShader(SHADER_FOLDER "polyMesh.vert", SHADER_FOLDER "flat.frag");
	spriteShader = BuildShader(SHADER_FOLDER "sprite.vert", SHADER_FOLDER "sprite.frag");
}

#pragma region Block
void setupBlockMesh(BlockMesh& mesh, bool onlyAllocBuffer, bool staticMesh) {
	const glm::vec3 faceVerts[] = {
	glm::vec3(0,0,0),
	glm::vec3(1,0,0),
	glm::vec3(1,0,1),
	glm::vec3(0,0,1),
	};

	const int faceIndices[] = {
		0, 1, 2, 2, 3, 0
	};

	GLuint VAO, VBO, instanceVBO, EBO;
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &instanceVBO);
	glGenBuffers(1, &EBO);

	mesh.VAO = VAO;
	mesh.VBO = VBO;
	mesh.EBO = EBO;
	mesh.instanceVBO = instanceVBO;

	int bufferMode;
	if (staticMesh) bufferMode = GL_STATIC_DRAW;
	else			bufferMode = GL_DYNAMIC_DRAW;

	// выбираем буфер как текущий
	glBindVertexArray(VAO);

	// instanced face
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);

	glBufferData(GL_ARRAY_BUFFER, sizeof(faceVerts), faceVerts, GL_STATIC_DRAW); // отправка вершин в память видеокарты
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(faceIndices), faceIndices, GL_STATIC_DRAW); // загружаем индексы

	// "объясняем" как необходимо прочитать массив с вершинами
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0); // pos
	glEnableVertexAttribArray(0);


	// instances
	glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(BlockFaceInstance) * mesh.faceSize, mesh.faces, GL_STATIC_DRAW);

	// pos
	glEnableVertexAttribArray(1);
	glVertexAttribIPointer(1, 1, GL_UNSIGNED_SHORT, sizeof(BlockFaceInstance), (void*)offsetof(BlockFaceInstance, pos));
	glVertexAttribDivisor(1, 1); // первая переменная - location атрибута в шейдере
	// face direction
	glEnableVertexAttribArray(2);
	glVertexAttribIPointer(2, 1, GL_BYTE, sizeof(BlockFaceInstance), (void*)offsetof(BlockFaceInstance, face));
	glVertexAttribDivisor(2, 1); // первая переменная - location атрибута в шейдере
	// texture id
	glEnableVertexAttribArray(3);
	glVertexAttribIPointer(3, 1, GL_UNSIGNED_SHORT, sizeof(BlockFaceInstance), (void*)offsetof(BlockFaceInstance, textureID));
	glVertexAttribDivisor(3, 1); // первая переменная - location атрибута в шейдере

	glBindVertexArray(0);
}

// обновить геометрию в ГПУ
void updateBlockMesh(BlockMesh& mesh) {
	glBindBuffer(GL_ARRAY_BUFFER, mesh.instanceVBO);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(BlockFaceInstance) * mesh.faceSize, mesh.faces);
	
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	mesh.needUpdate = false;
}

// установить параметры шейдера перед отрисовкой
void useCubeShader(glm::vec3 sunDir, glm::vec3 sunColor, glm::vec3 moonColor, glm::vec3 ambientColor,
	glm::mat4 projection, glm::mat4 view, glm::mat4 lightSpaceMatrix
)
{
	GLuint shader = cubeInstancedShader;
	glUseProgram(shader);

	// день
	if (sunDir.y > 0) {
		float sunIntencity = glm::max(sunDir.y, 0.0f);
		glm::vec3 sunShadingColor = sunColor * sunIntencity;
		glm::vec3 ambientShadingColor = ambientColor * glm::max(sunIntencity, 0.2f);

		glUniform3f(glGetUniformLocation(shader, "sunDir"), sunDir.x, sunDir.y, sunDir.z);
		glUniform3f(glGetUniformLocation(shader, "sunColor"), sunShadingColor.r, sunShadingColor.g, sunShadingColor.b);
		glUniform3f(glGetUniformLocation(shader, "ambientColor"), ambientShadingColor.r, ambientShadingColor.g, ambientShadingColor.b);
	}
	// ночь
	else {
		float sunIntencity = glm::max(-sunDir.y, 0.0f) * 0.3;
		glm::vec3 sunShadingColor = moonColor * sunIntencity;
		glm::vec3 ambientShadingColor = ambientColor * glm::max(sunIntencity, 0.2f);

		glUniform3f(glGetUniformLocation(shader, "sunDir"), -sunDir.x, -sunDir.y, -sunDir.z);
		glUniform3f(glGetUniformLocation(shader, "sunColor"), sunShadingColor.r, sunShadingColor.g, sunShadingColor.b);
		glUniform3f(glGetUniformLocation(shader, "ambientColor"), ambientShadingColor.r, ambientShadingColor.g, ambientShadingColor.b);
	}

	// установка переменных в вершинном шейдере
	glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
	glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(glGetUniformLocation(shader, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));

	glUniform1i(glGetUniformLocation(shader, "texture1"), 0);
	glUniform1i(glGetUniformLocation(shader, "shadowMap"), 1);
}

// локальная трансформация для модели
void cubeApplyTransform(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale) {
	glm::mat4 model = glm::mat4(1.0f); // единичная матрица (1 по диагонали)
	model = glm::translate(model, pos);
	model = glm::rotate(model, glm::radians(rot.x), glm::vec3(1.0, 0.0, 0.0));
	model = glm::rotate(model, glm::radians(rot.y), glm::vec3(0.0, 1.0, 0.0));
	model = glm::rotate(model, glm::radians(rot.z), glm::vec3(0.0, 0.0, 1.0));
	model = glm::scale(model, scale);
	glUniformMatrix4fv(glGetUniformLocation(cubeInstancedShader, "model"), 1, GL_FALSE, glm::value_ptr(model));
}

void drawBlockMesh(BlockMesh& mesh, const Texture& textureAtlas, GLuint shadowMap, glm::ivec2 chunkPos) {
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, textureAtlas.ID);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, shadowMap);
	glActiveTexture(GL_TEXTURE0);

	glUniform2i(glGetUniformLocation(cubeInstancedShader, "chunkPos"), chunkPos.x, chunkPos.y);
	glUniform2i(glGetUniformLocation(cubeInstancedShader, "atlasSize"), textureAtlas.width, textureAtlas.height);


	glBindVertexArray(mesh.VAO);
	glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, mesh.faceCount);

	glBindVertexArray(0);
}
#pragma endregion

#pragma region Sprite
void setupSprite(Sprite& sprite) {
	GLuint VAO, VBO, EBO;
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &EBO);

	sprite.VAO = VAO;
	sprite.VBO = VBO;
	sprite.EBO = EBO;

	// выбираем буфер как текущий
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);

	glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * sprite.vertexCount, sprite.vertices, GL_STATIC_DRAW); // отправка вершин в память видеокарты
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(Triangle) * sprite.trianglesCount, sprite.triangles, GL_STATIC_DRAW); // загружаем индексы

	// "объясняем" как необходимо прочитать массив с вершинами
	GLint stride = sizeof(Vertex);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, pos)); // pos
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, uv)); // tex coord

	glBindVertexArray(0);
}

// установить параметры шейдера спрайта перед отрисовкой
void useSpriteShader(glm::mat4 projection, glm::mat4 view) {
	GLuint shader = spriteShader;
	glUseProgram(shader);

	// установка переменных в вершинном шейдере
	glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
	glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, GL_FALSE, glm::value_ptr(view));
}

// локальная трансформация для спрайта
void spriteApplyTransform(glm::vec3 pos, float scale, bool spherical) {
	glUniform1f(glGetUniformLocation(spriteShader, "scale"), scale);
	if (spherical)
		glUniform1i(glGetUniformLocation(spriteShader, "spherical"), 1);
	else
		glUniform1i(glGetUniformLocation(spriteShader, "spherical"), 0);

	glm::mat4 model = glm::mat4(1.0f); // единичная матрица (1 по диагонали)
	model = glm::translate(model, pos);
	model = glm::scale(model, glm::vec3(scale, scale, scale));
	glUniformMatrix4fv(glGetUniformLocation(spriteShader, "model"), 1, GL_FALSE, glm::value_ptr(model));
}


void drawSprite(Sprite& sprite, GLuint texture) {

	glBindVertexArray(sprite.VAO);
	glBindTexture(GL_TEXTURE_2D, texture);
	glDrawElements(GL_TRIANGLES, sprite.trianglesCount * 3, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}
#pragma endregion

#pragma region PolyMesh
void polyMeshSetup(PolyMesh& mesh) {
	GLuint VAO, VBO, EBO;
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &EBO);

	mesh.VAO = VAO;
	mesh.VBO = VBO;
	mesh.EBO = EBO;

	// выбираем буфер как текущий
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);

	glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * mesh.vertexCapacity, mesh.vertices, GL_STATIC_DRAW); // отправка вершин в память видеокарты
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(Triangle) * mesh.triCapacity, mesh.triangles, GL_STATIC_DRAW); // загружаем индексы

	// "объясняем" как необходимо прочитать массив с вершинами
	GLint stride = sizeof(Vertex);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, pos)); // pos
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, uv)); // tex coord
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, color)); // color
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, normal)); // normal

	glBindVertexArray(0);
}

void polyMeshUseShader(glm::mat4 projection, glm::mat4 view, glm::mat4 lightSpaceMatrix)
{
	GLuint shader = polyMeshShader;
	glUseProgram(shader);

	// установка переменных в вершинном шейдере
	glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
	glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(glGetUniformLocation(shader, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));

	glUniform1i(glGetUniformLocation(shader, "texture1"), 0);
	glUniform1i(glGetUniformLocation(shader, "shadowMap"), 1);
}

void polyMeshApplyTransform(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale)
{
	glm::mat4 model = glm::mat4(1.0f); // единичная матрица (1 по диагонали)
	model = glm::translate(model, pos);
	model = glm::rotate(model, glm::radians(rot.x), glm::vec3(1.0, 0.0, 0.0));
	model = glm::rotate(model, glm::radians(rot.y), glm::vec3(0.0, 1.0, 0.0));
	model = glm::rotate(model, glm::radians(rot.z), glm::vec3(0.0, 0.0, 1.0));
	model = glm::scale(model, scale);
	glUniformMatrix4fv(glGetUniformLocation(polyMeshShader, "model"), 1, GL_FALSE, glm::value_ptr(model));
}

void polyMeshDraw(PolyMesh& mesh, GLuint texture, GLuint shadowMap, glm::vec3 sunDir, glm::vec3 sunColor, glm::vec3 ambientColor) {
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);
	
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, shadowMap);

	glActiveTexture(GL_TEXTURE0);

	glUniform1f(glGetUniformLocation(polyMeshShader, "lightingFactor"), 1.0f);
	glUniform3f(glGetUniformLocation(polyMeshShader, "sunDir"), sunDir.x, sunDir.y, sunDir.z);
	glUniform3f(glGetUniformLocation(polyMeshShader, "sunColor"), sunColor.x, sunColor.y, sunColor.z);
	glUniform3f(glGetUniformLocation(polyMeshShader, "ambientColor"), ambientColor.x, ambientColor.y, ambientColor.z);

	glBindVertexArray(mesh.VAO);
	glDrawElements(GL_TRIANGLES, mesh.triCount * 3, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}
#pragma endregion

#pragma region Flat
void useFlatShader(glm::mat4 projection, glm::mat4 view) {
	GLuint shader = flatShader;
	glUseProgram(shader);
	
	// установка переменных в вершинном шейдере
	glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
	glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, GL_FALSE, glm::value_ptr(view));
}

// локальная трансформация для модели
void flatApplyTransform(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale) {
	glm::mat4 model = glm::mat4(1.0f); // единичная матрица (1 по диагонали)
	model = glm::translate(model, pos);
	model = glm::rotate(model, glm::radians(rot.x), glm::vec3(1.0, 0.0, 0.0));
	model = glm::rotate(model, glm::radians(rot.y), glm::vec3(0.0, 1.0, 0.0));
	model = glm::rotate(model, glm::radians(rot.z), glm::vec3(0.0, 0.0, 1.0));
	model = glm::scale(model, scale);
	glUniformMatrix4fv(glGetUniformLocation(flatShader, "model"), 1, GL_FALSE, glm::value_ptr(model));
}

void drawFlat(PolyMesh& mesh, glm::vec3 color) {
	glUniform4f(glGetUniformLocation(flatShader, "color"), color.r, color.g, color.b, 1);

	glBindVertexArray(mesh.VAO);
	glDrawElements(GL_TRIANGLES, mesh.triCount * 3, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

void createSprite(Sprite& sprite, float scaleX, float scaleY, glm::vec2& uv, float sizeU, float sizeV) {
	Vertex* vertices = new Vertex[4] {
		Vertex(-0.5, -0.5, 0, uv.x, uv.y),
		Vertex(0.5, -0.5, 0, uv.x + sizeU, uv.y),
		Vertex(0.5, 0.5, 0, uv.x + sizeU, uv.y + sizeV),
		Vertex(-0.5, 0.5, 0, uv.x, uv.y + sizeV),
	};

	Triangle* triangles = new Triangle[2] {
		Triangle(0,1,2),
		Triangle(0,2,3)
	};

	sprite = {0};
	sprite.vertices = vertices;
	sprite.triangles = triangles;
	sprite.vertexCount = 4;
	sprite.trianglesCount = 2;
	setupSprite(sprite);
}
#pragma endregion
