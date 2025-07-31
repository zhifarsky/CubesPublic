#pragma once
#include "Typedefs.h"
#include "Mesh.h"

// chunk sizes
#define CHUNK_SX 16
#define CHUNK_SZ 16
#define CHUNK_SY 24
#define CHUNK_SIZE (CHUNK_SX * CHUNK_SZ * CHUNK_SY)

enum BlockType : u16 {
	btGround,
	btStone,
	btSnow,
	btIronOre,
	texSun,
	texMoon,
	btAir,
	btCOUNT // должен быть последним. обозначает количество типов блоков
};

struct Block {
	BlockType type;

	Block();
	Block(BlockType t);
};

struct Chunk {
	int posx, posz;
	Block* blocks;
	BlockMesh mesh;
	bool generated;
};

void meshChunk(Chunk& chunk);

#ifdef CHUNK_IMPL
Block::Block() {}

Block::Block(BlockType t) {
	this->type = t;
}

void meshChunk(Chunk& chunk) {
	int layerStride = CHUNK_SX * CHUNK_SZ;
	int stride = CHUNK_SX;
	int faceCount = 0;

	BlockFaceInstance* faces = chunk.mesh.faces;

	memset(faces, 0, chunk.mesh.faceSize * sizeof(BlockFaceInstance));

	int blockIndex = 0;
	Block* blocks = chunk.blocks;
	glm::vec3 color(1, 1, 1);
	for (size_t y = 0; y < CHUNK_SY; y++) {
		for (size_t z = 0; z < CHUNK_SZ; z++) {
			for (size_t x = 0; x < CHUNK_SX; x++) {
				BlockType blockType = blocks[blockIndex].type;
				if (blockType != btAir) {
					TextureID texID;
					switch (blockType)
					{
					case btGround:	texID = tidGround;	break;
					case btStone:	texID = tidStone;	break;
					case btSnow:	texID = tidSnow;	break;
					case btIronOre:	texID = tidIronOre;	break;
					default:	texID = tidGround;	break;
					}


					// top
					if (y == CHUNK_SY - 1 || blocks[blockIndex + layerStride].type == btAir) {
						faces[faceCount++] = BlockFaceInstance(blockIndex, faceYPos, texID);
					}

					// bottom
					if (y == 0 || blocks[blockIndex - layerStride].type == btAir) {
						faces[faceCount++] = BlockFaceInstance(blockIndex, faceYNeg, texID);
					}

					// front
					if (z == 0 || blocks[blockIndex - stride].type == btAir) {
						faces[faceCount++] = BlockFaceInstance(blockIndex, faceZPos, texID);
					}

					// back
					if (z == CHUNK_SZ - 1 || blocks[blockIndex + stride].type == btAir) {
						faces[faceCount++] = BlockFaceInstance(blockIndex, faceZNeg, texID);
					}

					// left
					if (x == 0 || blocks[blockIndex - 1].type == btAir) {
						faces[faceCount++] = BlockFaceInstance(blockIndex, faceXPos, texID);
					}

					// right
					if (x == CHUNK_SX - 1 || blocks[blockIndex + 1].type == btAir) {
						faces[faceCount++] = BlockFaceInstance(blockIndex, faceXNeg, texID);
					}
				}
				blockIndex++;
			}
		}
	}

	chunk.mesh.faceCount = faceCount;
}
#endif // CHUNK_IMPL
