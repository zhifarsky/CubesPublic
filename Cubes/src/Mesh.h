#pragma once
#include <iostream>
#include <glm.hpp>
#include <glad/glad.h>
#include "Typedefs.h"
#include "ResourceLoader.h"



void initShaders();

enum BlockFace : u8 {
	faceYPos = 0,
	faceYNeg = 1,
	faceXPos = 2,
	faceXNeg = 3,
	faceZPos = 4,
	faceZNeg = 5,
};

// в том же порядке, что и в атласе
enum TextureID : u16 {
	tidGround,
	tidStone,
	tidSun,
	tidMoon,
	tidSnow,
	tidIronOre,
	tidAir,
	tidCount
};

#pragma pack(push, 1)
struct BlockFaceInstance {
	u16 pos;
	TextureID textureID;
	BlockFace face;

	BlockFaceInstance(int pos, BlockFace face, TextureID textureID);
};
#pragma pack(pop)

// BLOCKS
// TODO: переименовать в BlockMesh
struct BlockMesh {
	BlockFaceInstance* faces;

	u32 faceCount;
	u32 faceSize;

	GLuint VAO, VBO, instanceVBO, EBO; // TODO: EBO и VBO должны быть глобальными, так как для всех мешей инстанциреум один и тот же полигон

	bool needUpdate;

	BlockMesh();
};

void setupBlockMesh(BlockMesh& mesh, bool onlyAllocBuffer = false, bool staticMesh = true);
void updateBlockMesh(BlockMesh& mesh);
void useCubeShader(glm::vec3 sunDir, glm::vec3 sunColor, glm::vec3 moonColor, glm::vec3 ambientColor,
	glm::mat4 projection, glm::mat4 view, glm::mat4 lightSpaceMatrix
);
void cubeApplyTransform(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale);
void drawBlockMesh(BlockMesh& mesh, const Texture& textureAtlas, GLuint shadowMap, glm::ivec2 chunkPos);

// POLYGONAL MESH
#pragma pack(push, 1)
struct Vertex {
	glm::vec3 pos;
	glm::vec2 uv;
	glm::vec3 color;
	glm::vec3 normal;

	Vertex();
	Vertex(float x, float y, float z);
	Vertex(float x, float y, float z, float u, float v);
	Vertex(float x, float y, float z, float u, float v, glm::vec3 color, glm::vec3 normal);
};
#pragma pack(pop)

struct Triangle {
	int indices[3];

	Triangle();
	Triangle(int a, int b, int c);
};

struct PolyMesh {
	Vertex* vertices;
	Triangle* triangles;

	int vertexCapacity, vertexCount;
	int triCapacity, triCount;

	GLuint VAO, VBO, EBO;
};

void polyMeshSetup(PolyMesh& mesh);
void polyMeshUseShader(glm::mat4 projection, glm::mat4 view, glm::mat4 lightSpaceMatrix);
void polyMeshApplyTransform(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale);
void polyMeshDraw(PolyMesh& mesh, GLuint texture, GLuint shadowMap, glm::vec3 sunDir, glm::vec3 sunColor, glm::vec3 ambientColor);

// SPRITE
struct Sprite {
	Vertex* vertices;
	Triangle* triangles;

	u32 vertexCount, trianglesCount;

	GLuint VAO, VBO, EBO;
};

void setupSprite(Sprite& sprite);
void createSprite(Sprite& sprite, float scaleX, float scaleY, glm::vec2& uv, float sizeU, float sizeV);
void useSpriteShader(glm::mat4 projection, glm::mat4 view);
void spriteApplyTransform(glm::vec3 pos, float scale, bool spherical = true);
void drawSprite(Sprite& sprite, GLuint texture);

// FLAT
void useFlatShader(glm::mat4 projection, glm::mat4 view);
void flatApplyTransform(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale);
void drawFlat(PolyMesh& mesh, glm::vec3 color);