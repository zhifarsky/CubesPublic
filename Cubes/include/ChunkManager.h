#pragma once
#include "Renderer.h"
#include "DataStructures.h"
#include "Items.h"

#define CHUNK_SX 16
#define CHUNK_SZ 16
#define CHUNK_SY 48
#define CHUNK_SIZE (CHUNK_SX * CHUNK_SZ * CHUNK_SY)
// TDOO: bounding box вместо радиуса

#define MIN_RENDER_DISTANCE 1
#define MAX_RENDER_DISTANCE 32
#define GetChunksSideCount(renderDistance) (renderDistance * 2 + 1)
#define GetChunksCount(renderDistance) (GetChunksSideCount(renderDistance) * GetChunksSideCount(renderDistance))
#define GetRenderDistance(chunksCount) (((int)sqrt(chunksCount) - 1) / 2)

struct ChunkManager;
extern ChunkManager g_chunkManager;
extern float g_chunkRadius;

struct Block {
	BlockType type;
};

enum class BlockSide : u8 {
	None = 0, YPos, YNeg, XPos, XNeg, ZPos, ZNeg
};

// TODO совмещение статусов?
// enum class ChunkStatus {
//  None = 0,
//  BlocksGenerated = 1 << 0,
//  BlocksAndMeshGenerated = 1 << 1,
//  ReadyToRender = 1 << 2,
//  ...
// };
// 
//if ((chunks[i].status & ChunkStatus::BlocksGenerated) ||
//	(chunks[i].status & ChunkStatus::BlocksAndMeshGenerated) ||
//	(chunks[i].status & ChunkStatus::ReadyToRender))
//{

enum class ChunkStatus : u8
{
	Uninitalized = 0,
	Initialized,
	Generated,		// blocks and mesh generated
	ReadyToRender	// sent to gpu memory
};

struct ChunkBlocks {
  Block b[CHUNK_SY][CHUNK_SZ][CHUNK_SX];
};

#define MAX_FACES_COUNT (CHUNK_SIZE * 6)

struct Chunk {
	GeometryInstanced mesh;
	int posx, posz;

	ChunkStatus status;
	bool generationInProgress;

  ChunkBlocks* blocks;
};

struct ChunkMeshGenResult {
	Chunk* chunk;
	
	BlockFaceInstance* faces;
	u32 facesCount;
};

void ChunkGenerateBlocks(Chunk* chunk, int posx, int posz, int seed);
ChunkMeshGenResult ChunkGenerateMesh(Arena* tempStorage, Chunk* chunk);
void ChunksSaveToDisk(Chunk* chunks, int chunksCount, const char* worldName);

#include <queue>
#include <mutex>
#include <functional>
#include <condition_variable>

struct ThreadState {
	Arena tempStorage;
};

// TODO: переписать и упростить
struct TaskQueue {
	std::queue <std::function<void(TaskQueue*, ThreadState*)>> queue;
	std::queue<ChunkMeshGenResult> genResultsQueue;
	std::vector<std::thread> threads;
	Array<ThreadState> threadsState;
	std::mutex mtx, genResultsMtx;
	std::condition_variable cv;
	bool stop;

	TaskQueue(int threadsCount) {
		threadsState.alloc(threadsCount);

		stop = false;
		for (size_t i = 0; i < threadsCount; i++)
		{
			threadsState.append({ 0 });
			ThreadState* state = &threadsState[i];
			state->tempStorage.alloc(Megabytes(16), Gigabytes(1));

			threads.emplace_back([this, state]() {
				while (true) {
					// get new task
					std::function<void(TaskQueue*,ThreadState*)> task;
					{
						std::unique_lock<std::mutex> lock(mtx);
						cv.wait(lock, [this]() {return stop || queue.size() > 0; });

						if (stop)
							break;

						task = std::move(queue.front());
						queue.pop();
					}

					// execute task
					task(this, state); // TODO: передавать в задачу id потока дл€ доступа к состо€нию потока threadsState[threadID]
					// arena cleanup
					state->tempStorage.clear();
				}
				});
		}
	}

	~TaskQueue() {
		stop = true;
		cv.notify_all();

		for (size_t i = 0; i < threads.size(); i++)
		{
			threads[i].join();
		}

		for (size_t i = 0; i < threadsState.count; i++)
		{
			threadsState[i].tempStorage.release();
		}
		threadsState.release();
	}

	void AddTask(std::function<void(TaskQueue*, ThreadState*)>&& task) {
		std::lock_guard lock(mtx);
		queue.push(std::move(task));
		cv.notify_one();
	}

	bool IsResultsEmpty() {
		std::lock_guard lock(genResultsMtx);
		return genResultsQueue.empty();
	}

	void PushResult(ChunkMeshGenResult result) {
		std::lock_guard lock(genResultsMtx);
		genResultsQueue.push(result);
	}

	ChunkMeshGenResult PopResult() {
		std::lock_guard lock(genResultsMtx);
		ChunkMeshGenResult result = genResultsQueue.front();
		genResultsQueue.pop();
		return result;
	}

	void Clear() {
		std::lock_guard lock(mtx);
		while (!queue.empty()) {
			queue.pop();
		}
	}

	//void StopAndJoin() {
	//	stop = true;
	//	cv.notify_all();
	//	for (size_t i = 0; i < threads.size(); i++)
	//	{
	//		threads[i].join();
	//	}
	//	threads.clear();
	//	threadsState.clear();
	//}
};


struct ChunkManager {
	Chunk* chunks;
  ChunkBlocks* chunksBlocks;
	TaskQueue* queue;
	int chunksCount;
	int seed;
};

struct PlaceBlockResult {
	glm::ivec3 pos;
	BlockType typePrev;
	BlockType typeNew;
	bool success;
};

struct PeekBlockResult {
	Block* block; // NOTE: небезопасно хранить результат, т.к. указатель может быть невалиден
	glm::ivec3 blockPos;
	int chunkIndex;
	bool success;
};

void ChunkManagerCreate(int seed);
void ChunkManagerAllocChunks(GameMemory* memory, ChunkManager* manager, u32 renderDistance);
void ChunkManagerReleaseChunks(GameMemory* memory, ChunkManager* manager);
void ChunkManagerBuildChunks(ChunkManager* manager, Arena* tempStorage, Frustum* frustum, float playerPosX, float playerPosZ);
Block* ChunkManagerPeekBlockFromPos(ChunkManager* manager, float posX, float posY, float posZ, int* outChunkIndex = NULL);
PeekBlockResult ChunkManagerPeekBlockFromRay(ChunkManager* manager, glm::vec3 rayPos, glm::vec3 rayDir, u8 maxDist);
PlaceBlockResult ChunkManagerPlaceBlock(Arena* tempStorage, ChunkManager* manager, BlockType blockType, glm::vec3 pos, glm::vec3 direction, u8 maxDist);
PlaceBlockResult ChunkManagerDestroyBlock(Arena* tempStorage, ChunkManager* manager, glm::vec3 pos, glm::vec3 direction, u8 maxDist);
glm::ivec2 PosToChunkPos(glm::vec3 pos);
