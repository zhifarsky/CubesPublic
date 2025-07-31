#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <gtc/noise.hpp>
#include <gtc/matrix_transform.hpp>
#include "World.h"
#include "FastNoiseLite.h"

static FastNoiseLite noise;


void Display::update(GLFWwindow* window) {
		glfwGetFramebufferSize(window, &displayWidth, &displayHeight);
}

glm::mat4 getProjection(float FOV, int displayW, int displayH) {
	return glm::perspective(glm::radians(FOV), (float)displayW / (float)displayH, 0.1f, 1000.0f);
}

int getChunksCount(int renderDistance) {
	int chunkSide = renderDistance * 2 + 1;
	return chunkSide * chunkSide;
}

void Camera::update(float yaw, float pitch) {
	glm::vec3 direction;
	direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	direction.y = sin(glm::radians(pitch));
	direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	front = glm::normalize(direction);
}

void GameWorld::init(u32 seed, u32 chunksCount) {
	//chunks = (Chunk*)malloc(sizeof(Chunk) * chunksCount);
	chunks = (Chunk*)calloc(chunksCount, sizeof(Chunk));
	this->chunksCount = chunksCount;
	this->seed = seed;

	noise = FastNoiseLite(seed);
}

void GameWorld::reallocChunks(u32 chunksCount) {
	chunks = (Chunk*)realloc(chunks, sizeof(Chunk) * chunksCount);
	this->chunksCount = chunksCount;
}

float GameWorld::perlinNoise(glm::vec2 pos, int seedShift) {
	if (seedShift != 0)
		noise.SetSeed(seed + seedShift);
	noise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
	return noise.GetNoise(pos.x, pos.y);
}

float GameWorld::perlinNoise(glm::vec3 pos, int seedShift) {
	if (seedShift != 0)
		noise.SetSeed(seed + seedShift);
	noise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
	return noise.GetNoise(pos.x, pos.y, pos.z);
}

void GameWorld::generateChunk(int index, int posx, int posz) {
	Chunk& chunk = chunks[index];
	chunk.generated = false;

	chunk.posx = posx;
	chunk.posz = posz;

	float noiseScale = 6.0f;
	float caveNoiseScale = 5.0f;
	float temperatureNoiseScale = 0.7f;
	
	// определение типов блоков
	Block* blocks = chunk.blocks;
	int blockIndex = 0;
	for (size_t y = 0; y < CHUNK_SY; y++) {
		for (size_t z = 0; z < CHUNK_SZ; z++) {
			for (size_t x = 0; x < CHUNK_SX; x++) {
				// biome
				BlockType groundBlockType;
				float temperature = perlinNoise(glm::vec2((int)x + posx, (int)z + posz) * temperatureNoiseScale);
				float biomeEdge = 0.5;
				float riverWidth = 0.06;
				temperature = (temperature + 1.0f) / 2.0f;
				if (temperature > biomeEdge)
					groundBlockType = btGround;
				else
					groundBlockType = btSnow;
				
				// height
				float height = perlinNoise(glm::vec2((int)x + posx, (int)z + posz) * noiseScale);
				height = (height + 1.0f) / 2.0f;

				// generate rivers between biomes
				if (temperature > biomeEdge && temperature - riverWidth <= biomeEdge) {
					height *= 0.15;
					height += 0.2;
				}

				// generate height
				if (y > height * 23.0f)
					blocks[blockIndex].type = btAir;
				else if (y > height * 20.0f) {
					blocks[blockIndex].type = groundBlockType;
				}
				else
				{
					blocks[blockIndex].type = btStone;
					
					float ironOre = perlinNoise(glm::vec3((int)x + posx, (int)y, (int)z + posz) * 30.0f, 1);
					ironOre = (ironOre + 1.0f) / 2;
					if (ironOre > 0.7)
						blocks[blockIndex].type = btIronOre;

					// generate caves
					float cave = perlinNoise(glm::vec3((int)x + posx, y, (int)z + posz) * caveNoiseScale);
					float caveWidth = 0.06;
					cave = (cave + 1.0f) / 2.0f;
					if (cave > 0.5 && cave < 0.5 + caveWidth) {
						blocks[blockIndex].type = btAir;
					}
				}

				// bedrock
				if (y == 0) {
					blocks[blockIndex].type = btStone;
				}

				//blocks[blockIndex].brightness = (float)y / (float)chunkSizeY;
				blockIndex++;
			}
		}
	}

	chunk.generated = true;
}

Block* GameWorld::peekBlockFromPos(glm::vec3 pos) {
	int chunkIndex = -1;
	
	// поиск чанка, в котором находится этот блок
	// TODO: более эффективный метод, чем перебор?
	for (size_t i = 0; i < chunksCount; i++)
	{
		if (pos.x >= chunks[i].posx && pos.x < chunks[i].posx + CHUNK_SX &&
			pos.z >= chunks[i].posz && pos.z < chunks[i].posz + CHUNK_SZ) {
			chunkIndex = i;
			break;
		}
	}
	if (chunkIndex == -1) 
		return NULL;

	glm::vec3 relativePos(
		pos.x - chunks[chunkIndex].posx, 
		pos.y, 
		pos.z - chunks[chunkIndex].posz);

	glm::floor(relativePos);

	static int strideZ = CHUNK_SX;
	static int strideY = CHUNK_SX * CHUNK_SZ;
	int shift = (int)relativePos.x + (int)relativePos.z * strideZ + (int)relativePos.y * strideY;
	if (shift >= CHUNK_SIZE) { // выход за пределы чанка
		return NULL;
	}

	return &chunks[chunkIndex].blocks[shift];
}

Block* GameWorld::peekBlockFromRay(glm::vec3 rayPos, glm::vec3 rayDir, u8 maxDist, glm::vec3* outBlockPos) {
	if (glm::length(rayDir) == 0)
		return NULL;
	float deltaDist = 1;
	float dist = 0;

	Block* block;

	glm::vec3 norm = glm::normalize(rayDir);
	while (dist < maxDist) {
		glm::vec3 currentPos = rayPos + (norm * dist);

		block = peekBlockFromPos(
			glm::vec3(
			currentPos.x,
			currentPos.y,
			currentPos.z));

		if (block != NULL && block->type != btAir) {
			if (outBlockPos)
				*outBlockPos = glm::floor(glm::vec3(currentPos.x, currentPos.y, currentPos.z));
			return block;
		}

		dist += deltaDist;
	}
	return NULL;	
}