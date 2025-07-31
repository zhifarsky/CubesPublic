#pragma region external dependencies
#define NOMINMAX
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <SOIL/SOIL.h>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>
#include <gtc/noise.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_stdlib.h>
#pragma endregion
#pragma region internal dependencies
#define CHUNK_IMPL
#include "ResourceLoader.h"
#include "Typedefs.h"
#include "Tools.h"
#include "Mesh.h"
#include "ui.h"
#include "Directories.h"
#include "Entity.h"
#include "DataStructures.h"
#include "Chunk.h"
#include "World.h"
#pragma endregion

// количество чанков
int renderDistance = 12;
int chunksSide = renderDistance * 2 + 1;
int chunksCount = chunksSide * chunksSide;

float near_plane = 1.0f, far_plane = 500.0f;
float projDim = 64.0f;
float shadowLightDist = 100;

GameWorld gameWorld;
Display display;
Player player;

static float lastX = 400, lastY = 300; // позиция курсора
static float yaw = 0, pitch = 0;
static bool cursorMode = false; // TRUE для взаимодействия с UI, FALSE для перемещения камеры

static DynamicArray<Entity> entities;

struct GuiArgs {
	bool* wireframe_cb, *debugView_cb, *vsyncOn;
	float* col_mix_slider, *fov_slider, *camspeed_slider;
	int* chunksUpdated;
	glm::vec3* sunDir;
	glm::vec3 *sunColor, * ambientColor;
	glm::vec2 currentChunkPos;
};

static void cubes_gui(GuiArgs& args);

static void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
	if (cursorMode == true) {
		ImGui_ImplGlfw_CursorPosCallback(window, xpos, ypos);
		lastX = xpos;
		lastY = ypos;
		return;
	}

	float xoffset = xpos - lastX;
	float yoffset = lastY - ypos; // reversed since y-coordinates range from bottom to top
	lastX = xpos;
	lastY = ypos;


	const float sensitivity = 0.1f;
	xoffset *= sensitivity;
	yoffset *= sensitivity;

	yaw += xoffset;
	pitch += yoffset;

	if (pitch > 89.0f)
		pitch = 89.0f;
	if (pitch < -89.0f)
		pitch = -89.0f;

}

struct GameInputs {
	bool attack;
	bool placeBlock;

	void clear() {
		attack = false;
		placeBlock = false;
	}
};

GameInputs gameInputs;

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
	if (cursorMode == true) {
		ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
		return;
	}

	switch (button) {
	case GLFW_MOUSE_BUTTON_RIGHT:
		if (action == GLFW_PRESS) {
			gameInputs.placeBlock = true;
		}
		break;
	case GLFW_MOUSE_BUTTON_LEFT:
		if (action == GLFW_PRESS) {
			gameInputs.attack = true;
		}
		break;
	default:
		break;
	}
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (key == GLFW_KEY_Q && action == GLFW_RELEASE) {
		if (cursorMode == false) {
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			cursorMode = true;
		}
		else {
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			cursorMode = false;
		}
	}
}

enum uiElemType : u16 {
	uiInventoryCell,
	uiHeart,
	uiCross,
	uiCOUNT
};

struct UVOffset {
	glm::vec2 offset;
	glm::vec2 scale;

	UVOffset() {};
	UVOffset(glm::vec2 offset, glm::vec2 scale) {
		this->offset = offset;
		this->scale = scale;
	}
};

float sunSpeed = 0.2;
glm::vec3 sunDir(0, 0, 0);
glm::vec3 moonDir(0, 0, 0);
float isDay = true;
glm::vec3 sunColor(1, 0.9, 0.8);
glm::vec3 moonColor(0.6, 0.6, 0.9);
glm::vec3 ambientColor(0.4, 0.4, 0.8);

//static glm::vec2 blocksUV[btCOUNT];
static UVOffset blocksUV[btCOUNT];
static UVOffset uiUV[uiCOUNT];
static float texUSize;
static float texVSize;


Chunk* chunks = 0;

#define ENABLE_LIGHTING 0
void updateLighting(Chunk& chunk) {
	static float brightnessDelta = 1.0f / 6.0f * 0.3; // затемнение за 1 блок поблизости
	int layerStride = CHUNK_SX * CHUNK_SZ;
	int stride = CHUNK_SX;
	Block* blocks = chunk.blocks;
#if ENABLE_LIGHTING

	for (size_t i = 0; i < CHUNK_SIZE; i++) {
		if (blocks[i].type != btAir) {
			float brightness = 1;
			
			if (i + 1 < CHUNK_SIZE 
				&& blocks[i + 1].type != btAir)
				brightness -= brightnessDelta;

			if (i > 0 
				&& blocks[i - 1].type != btAir)
				brightness -= brightnessDelta;

			if (i + layerStride < CHUNK_SIZE 
				&& blocks[i + layerStride].type != btAir)
				brightness -= brightnessDelta;

			if (i - layerStride >= 0 
				&& blocks[i - layerStride].type != btAir)
				brightness -= brightnessDelta;

			if (i - stride >= 0 && blocks[i - stride].type != btAir)
				brightness -= brightnessDelta;

			if (i + stride < CHUNK_SIZE
				&& blocks[i + stride].type != btAir)
				brightness -= brightnessDelta;


			blocks[i].brightness = brightness;
		}
	}
#else
	for (size_t i = 0; i < CHUNK_SIZE; i++) {
		//blocks[i].brightness = 1;
	}
#endif
}

struct ChunkGenTask {
	int posx;
	int posz;
	int index;
};
struct WorkingThread {
	HANDLE handle;
	int threadID;
};
WorkQueue chunkGenQueue(CHUNK_SIZE);
ChunkGenTask chunkGenTasks[CHUNK_SIZE];
WorkingThread* chunkGenThreads = 0;


DWORD chunkGenThreadProc(WorkingThread* args) {
	for (;;) {
		QueueTaskItem queueItem = chunkGenQueue.getNextTask();
		if (queueItem.valid) {
			ChunkGenTask* task = &chunkGenTasks[queueItem.taskIndex];
			dbgprint("chunk (%d, %d)\n", task->posx, task->posz);
			
			gameWorld.generateChunk(task->index, task->posx, task->posz);
			meshChunk(gameWorld.chunks[task->index]);
			gameWorld.chunks[task->index].mesh.needUpdate = true; // необходимо отправить новый меш на ГПУ в основоном потоке

			chunkGenQueue.setTaskCompleted();
		}
		else
			WaitForSingleObject(chunkGenQueue.semaphore, INFINITE);
	}

	return 0;
}

void updateChunk(int chunkIndex, int posx, int posz) {
	//dbgprint("generating chunk %d: (%d,%d)\n", posx, posz);
	chunkGenTasks[chunkGenQueue.taskCount].posx = posx;
	chunkGenTasks[chunkGenQueue.taskCount].posz = posz;
	chunkGenTasks[chunkGenQueue.taskCount].index = chunkIndex;
	chunkGenQueue.addTask();
}

enum CubeSide : u8 {
	cubeNone,
	cubeFront,
	cubeBack,
	cubeRight,
	cubeLeft,
	cubeBottom,
	cubeTop,
};

CubeSide BlockSideCastRay(glm::vec3& rayOrigin, glm::vec3& rayDir, glm::vec3& cubePos) {
	glm::vec3 invSlope = 1.0f / rayDir;
	glm::vec3 boxMin = cubePos;
	glm::vec3 boxMax = cubePos + 1.0f;

	float tx1 = (boxMin.x - rayOrigin.x) * invSlope.x;
	float tx2 = (boxMax.x - rayOrigin.x) * invSlope.x;

	float tmin = std::min(tx1, tx2);
	float tmax = std::max(tx1, tx2);

	float ty1 = (boxMin.y - rayOrigin.y) * invSlope.y;
	float ty2 = (boxMax.y - rayOrigin.y) * invSlope.y;

	tmin = std::max(tmin, std::min(ty1, ty2));
	tmax = std::min(tmax, std::max(ty1, ty2));


	float tz1 = (boxMin.z - rayOrigin.z) * invSlope.z;
	float tz2 = (boxMax.z - rayOrigin.z) * invSlope.z;

	tmin = std::max(tmin, std::min(tz1, tz2));
	tmax = std::min(tmax, std::max(tz1, tz2));

	glm::vec3 pos(
		(tmin * rayDir.x) + rayOrigin.x,
		(tmin * rayDir.y) + rayOrigin.y,
		(tmin * rayDir.z) + rayOrigin.z
	);

	glm::vec3 relPos = pos - cubePos;

	float e = 0.001;

	if (relPos.y > 1 - e)
		return cubeTop;
	if (relPos.y < 0 + e)
		return cubeBottom;
	if (relPos.z < 0 + e)
		return cubeFront;
	if (relPos.z > 1 - e)
		return cubeBack;
	if (relPos.x < 0 + e)
		return cubeLeft;
	if (relPos.x > 1 - e)
		return cubeRight;

	return cubeNone;
}


void CubesMainGameLoop(GLFWwindow* window) {
#if 0
	Chunk testChunk;
	{
		testChunk.posx = 0;
		testChunk.posz = 0;
		testChunk.mesh.faceSize = CHUNK_SIZE * 6;
		testChunk.mesh.faces = (BlockFaceInstance*)calloc(CHUNK_SIZE * 6, sizeof(BlockFaceInstance));

		testChunk.blocks = (Block*)calloc(CHUNK_SIZE, sizeof(Block));

		for (size_t i = 0; i < CHUNK_SIZE; i++)
		{
			testChunk.blocks[i] = Block(btAir);
		}

		for (size_t i = 0; i < CHUNK_SX * CHUNK_SZ; i++)
		{
			testChunk.blocks[i] = Block(btGround);
		}

		testChunk.blocks[70 + (CHUNK_SX * CHUNK_SZ) * 3] = Block(btGround);
		testChunk.blocks[70 + (CHUNK_SX * CHUNK_SZ) * 4] = Block(btGround);

		meshChunk(testChunk);
		setupBlockMesh(testChunk.mesh);

	}
#endif
	
	// GUI
	float col_mix_slider = 1.0, scale_slider = 1, fov_slider = 80, pulsation_slider = 0.3;
	float rotation_slider[3] = { 0, 0, 0 }, trans_slider[3] = { 0 };
	bool wireframe_cb = false;
	bool debugView_cb = true;
	int chunksUpdated = 0;

	gameWorld.init(0, chunksCount);
	chunks = gameWorld.chunks;

	player.camera.pos = glm::vec3(8, 30, 8);
	player.camera.front = glm::vec3(0, 0, -1);
	player.camera.up = glm::vec3(0, 1, 0);
	player.speed = 20;
	
	// компиляция шейдеров
	initShaders();
	uiInit();
	// потоки для создания чанков
	{
		int threadCount = 16;
		chunkGenThreads = (WorkingThread*)malloc(sizeof(WorkingThread) * threadCount);
		for (size_t i = 0; i < threadCount; i++) {
			chunkGenThreads[i].threadID = i;
			chunkGenThreads[i].handle = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)chunkGenThreadProc, &chunkGenThreads[i], 0, 0);
		}
	}
	

	// загрузка текстур
	Texture testTexture = LoadTexture(TEX_FOLDER "uv.png", textureRGBA);
	Texture textureAtlas = LoadTexture(TEX_FOLDER "TextureAtlas.png", textureRGBA);
	{
		texUSize = 16.0 / (float)textureAtlas.width;
		texVSize = 16.0 / (float)textureAtlas.height;
		blocksUV[btGround].offset = { 0, 0 };
		blocksUV[btGround].scale = { texUSize, texVSize };
		blocksUV[btStone].offset = { texUSize, 0 };
		blocksUV[btStone].scale = { texUSize, texVSize };
		blocksUV[texSun].offset = { texUSize * 2, 0 };
		blocksUV[texSun].scale = { texUSize, texVSize };
		blocksUV[texMoon].offset = { texUSize * 3, 0 };
		blocksUV[texMoon].scale = { texUSize, texVSize };
	}
	// TODO: добавить координаты для ui элементов, отрендерить на экран
	Texture uiAtlas = LoadTexture(TEX_FOLDER "UiAtlas.png", textureRGBA);
	{
		glm::vec2 tileSize(16.0f / (float)uiAtlas.width, 16.0f / (float)uiAtlas.height);
		glm::vec2 tileOrigin(0, 0);

		for (uiElemType elemType : {uiInventoryCell, uiHeart, uiCross})
		{
			uiUV[elemType] = UVOffset(tileOrigin, tileSize);
			tileOrigin.x += tileSize.x;
		}
	}

	for (size_t i = 0; i < chunksCount; i++)
	{
		chunks[i].blocks = new Block[CHUNK_SX * CHUNK_SY * CHUNK_SZ]; // TODO: аллокация в арене
		chunks[i].mesh.faceSize = CHUNK_SIZE * 6;
		chunks[i].mesh.faces = (BlockFaceInstance*)calloc(chunks[i].mesh.faceSize, sizeof(BlockFaceInstance)); // 6 сторон по 4 вершины
		setupBlockMesh(chunks[i].mesh, false, true);
	}
	// генерация начальных чанков
	{
		int chunkNum = 0;
		for (int z = -renderDistance; z <= renderDistance; z++) {
			for (int x = -renderDistance; x <= renderDistance; x++) {
				updateChunk(chunkNum, x * CHUNK_SX, z * CHUNK_SZ);
				chunkNum++;
			}
		}
		chunkGenQueue.waitAndClear();

		for (size_t i = 0; i < chunksCount; i++) {
			if (gameWorld.chunks[i].mesh.needUpdate) {
				updateBlockMesh(gameWorld.chunks[i].mesh);
				//gameWorld.chunks[i].ready = true;
			}
		}
	}

	PolyMesh box;
	Sprite sunSprite, moonSprite;
	{
		Vertex* vertices = new Vertex[8]{
			Vertex(0.0f, 0.0f, 0.0f, 0.0f, 0.0f),
			Vertex(1.0f, 0.0f, 0.0f, 1.0f, 0.0f),
			Vertex(1.0f, 1.0f, 0.0f, 1.0f, 1.0f),
			Vertex(0.0f, 1.0f, 0.0f, 0.0f, 1.0f),

			Vertex(0.0f, 0.0f, 1.0f, 0.0f, 1.0f),
			Vertex(1.0f, 0.0f, 1.0f, 1.0f, 1.0f),
			Vertex(1.0f, 1.0f, 1.0f, 1.0f, 0.0f),
			Vertex(0.0f, 1.0f, 1.0f, 0.0f, 0.0f),
		};
		for (size_t i = 0; i < 8; i++)
		{
			vertices[i].normal = { 0,1,0 };
		}
		Triangle* triangles = new Triangle[12]{
			Triangle(0, 2, 1), Triangle(0, 3, 2),
			Triangle(4, 5, 7), Triangle(5, 6, 7),
			Triangle(0, 4, 3), Triangle(4, 7, 3),
			Triangle(1, 2, 6), Triangle(1, 6, 5),
			Triangle(2, 3, 6), Triangle(3, 7, 6),
			Triangle(0, 1, 4), Triangle(1, 5, 4)
		};

		box.vertices = vertices;
		box.triangles = triangles;
		box.vertexCount = 8;
		box.vertexCapacity = 8;
		box.triCount = 12;
		box.triCapacity = 12;

		polyMeshSetup(box);

		createSprite(sunSprite, 1, 1, blocksUV[texSun].offset, texUSize, texVSize);
		createSprite(moonSprite, 1, 1, blocksUV[texMoon].offset, texUSize, texVSize);
	}

	float deltaTime = 0;
	float lastFrame = 0;

	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetKeyCallback(window, key_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	glEnable(GL_DEPTH_TEST);
	// TODO: включить
	glEnable(GL_CULL_FACE);

	{
		Entity entity;
		entity.type = entityZombie;
		entity.state = entityStateIdle;
		entity.pos = { 0, CHUNK_SY-2, 0 };
		entities.append(entity);

		entity.type = entityZombie;
		entity.state = entityStateIdle;
		entity.pos = { 5, CHUNK_SY-2, 5 };
		entities.append(entity);
	}

	// shadow framebuffer
	GLuint shadowShader, polyMeshShadowShader;
	GLuint depthMapFBO, depthMap;
	u32 SHADOW_WIDTH = 1024 * 2, SHADOW_HEIGHT = 1024 * 2;
	{
		glGenFramebuffers(1, &depthMapFBO);

		glGenTextures(1, &depthMap);
		glBindTexture(GL_TEXTURE_2D, depthMap);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
			SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
		glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

		glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		shadowShader = BuildShader(SHADER_FOLDER "blockDepthShader.vert", SHADER_FOLDER "depthShader.frag");
		polyMeshShadowShader = BuildShader(SHADER_FOLDER "polyMeshDepthShader.vert", SHADER_FOLDER "depthShader.frag");
	}

	// MAIN GAME LOOP
	//int lastChunkPosX = INT_MAX, lastChunkPosZ = INT_MAX;
	int lastChunkPosX = (int)(player.camera.pos.x / CHUNK_SX) * CHUNK_SX, 
		lastChunkPosZ = (int)(player.camera.pos.z / CHUNK_SZ) * CHUNK_SZ;
	while (!glfwWindowShouldClose(window))
	{
		float currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		gameInputs.clear();

		glfwPollEvents();
		
		// обновление направления солнца
#if 1
		sunDir.x = cos(currentFrame * sunSpeed);
		sunDir.y = sin(currentFrame * sunSpeed);
		sunDir.z = 0;
#endif
		moonDir = -sunDir;
		isDay = (sunDir.y > 0);
		
		int currentChunkPosX = (int)(player.camera.pos.x / CHUNK_SX) * CHUNK_SX;
		int currentChunkPosZ = (int)(player.camera.pos.z / CHUNK_SZ) * CHUNK_SZ;
		if (player.camera.pos.x < 0)
			currentChunkPosX -= CHUNK_SX;
		if (player.camera.pos.z < 0)
			currentChunkPosZ -= CHUNK_SZ;

		// генерация новых чанков
#if 1
		if (lastChunkPosX != currentChunkPosX || lastChunkPosZ != currentChunkPosZ) {
			chunkGenQueue.clearTasks();
			int chunkNum = 0;
			for (int z = -renderDistance; z <= renderDistance; z++) {
				for (int x = -renderDistance; x <= renderDistance; x++) {
					glm::ivec2 newChunkPos(currentChunkPosX + x * CHUNK_SX, currentChunkPosZ + z * CHUNK_SZ);
					
					bool alreadyGenerated = false;
					for (size_t i = 0; i < chunksCount; i++) {
						// если чанк уже есть среди сгенерированных, пропускаем генерацию
						if (chunks[i].posx == newChunkPos.x && chunks[i].posz == newChunkPos.y) {
							alreadyGenerated = true;
							break;
						}
					}
					if (alreadyGenerated)
						continue;

					int chunkToReplaceIndex = -1;
					for (size_t i = 0; i < chunksCount; i++) {
						// если чанк за пределом видимости
						if (abs(chunks[i].posx - currentChunkPosX) > renderDistance * CHUNK_SX ||
							abs(chunks[i].posz - currentChunkPosZ) > renderDistance * CHUNK_SZ) {
							chunkToReplaceIndex = i;
							//dbgprint("chunkToReplaceIndex %d\n", chunkToReplaceIndex);
							break;
						}
					}

					if (chunkToReplaceIndex == -1)
						continue;

					dbgprint("Replacing chunk #%d (%d, %d) with (%d, %d)\n", 
						chunkToReplaceIndex, chunks[chunkToReplaceIndex].posx, chunks[chunkToReplaceIndex].posx,
						newChunkPos.x, newChunkPos.y);

					chunks[chunkToReplaceIndex].posx = newChunkPos.x;
					chunks[chunkToReplaceIndex].posz = newChunkPos.y;
					updateChunk(chunkToReplaceIndex, newChunkPos.x, newChunkPos.y);
					chunkNum++;
				}
			}

		}
#endif

		// отправляем меши чанков на ГПУ если необходимо (генерация мешей на отдельных потоках)
		{
			static int i = 0;
			if (i < chunksCount) {
				if (gameWorld.chunks[i].mesh.needUpdate) {
					updateBlockMesh(gameWorld.chunks[i].mesh);
					//gameWorld.chunks[i].generated = true;
				}
				i++;
			}
			else
				i = 0;
		}

		lastChunkPosX = currentChunkPosX;
		lastChunkPosZ = currentChunkPosZ;

		// find current chunk
		Chunk* chunk = &chunks[0];
		BlockMesh* chunkMesh = &chunk->mesh;
		for (size_t i = 0; i < chunksCount; i++)
		{
			if (player.camera.pos.x >= chunks[i].posx && player.camera.pos.x <= chunks[i].posx + CHUNK_SX
				&& player.camera.pos.z >= chunks[i].posz && player.camera.pos.z <= chunks[i].posz + CHUNK_SZ) {
				chunk = &chunks[i];
				chunkMesh = &chunk->mesh;
				break;
			}
		}

		// update entities
		for (size_t i = 0; i < entities.count; i++)
		{
			Entity& entity = entities.items[i];

			if (entity.type == entityNull)
				continue;

			// gravity
			entity.pos.y -= 5 * deltaTime;
			entity.pos += entity.speed * deltaTime;
			entity.speed *= 0.1;
			Block* belowBlock;
			do {
				belowBlock = gameWorld.peekBlockFromPos(glm::vec3(entity.pos.x, entity.pos.y - 1, entity.pos.z));
				if (belowBlock && belowBlock->type != btAir)
					entity.pos.y = (int)entity.pos.y + 1;
				else
					break;
			} while (belowBlock);
			
			if (glm::distance(entity.pos, player.camera.pos) < 16) {
				entity.state = entityStateChasing;
			}
			else {
				entity.state = entityStateIdle;
			}

			switch (entities.items[i].state) {
			case entityStateChasing:
				glm::vec3 toPlayer = (player.camera.pos - entity.pos) * glm::vec3(1,0,1);
				if (glm::length(toPlayer) > 0) {
					entities.items[i].pos += glm::normalize(toPlayer) * 3.0f * deltaTime;
				}
				break;
			case entityStateIdle:
				break;
			}
		}

		int display_w, display_h;
		glfwGetFramebufferSize(window, &display_w, &display_h);

#pragma region Отрисовка
#pragma region трансформации
		float cameraSpeedAdj = player.speed * deltaTime; // делаем скорость камеры независимой от FPS

		// трансформация вида (камера)
		glm::vec3 direction;
		glm::mat4 view, projection;
		{
			direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
			direction.y = sin(glm::radians(pitch));
			direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
			player.camera.front = glm::normalize(direction);

			if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
				player.camera.pos += cameraSpeedAdj * player.camera.front;
			if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
				player.camera.pos -= cameraSpeedAdj * player.camera.front;
			if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
				player.camera.pos -= glm::normalize(glm::cross(player.camera.front, player.camera.up)) * cameraSpeedAdj;
			if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
				player.camera.pos += glm::normalize(glm::cross(player.camera.front, player.camera.up)) * cameraSpeedAdj;

			view = glm::lookAt(player.camera.pos, player.camera.pos + player.camera.front, player.camera.up);

			// матрица проекции (перспективная/ортогональная проекция)
			projection = glm::mat4(1.0);
			projection = glm::perspective(glm::radians(fov_slider), (float)display_w / (float)display_h, 0.1f, 1000.0f);
		}
#pragma endregion

		// rendering shadow maps
		glm::mat4 lightSpaceMatrix;
		{
			//glCullFace(GL_FRONT);

			glm::mat4 lightProjection = glm::ortho(-projDim, projDim, -projDim, projDim, near_plane, far_plane);
			glm::vec3 lightDir = isDay ? sunDir : moonDir;
			glm::mat4 lightView = glm::lookAt(
				shadowLightDist * lightDir + player.camera.pos * glm::vec3(1, 0, 1),
				player.camera.pos * glm::vec3(1,0,1),
				glm::vec3(0.0f, 1.0f, 0.0f));
			

			lightSpaceMatrix = lightProjection * lightView;

			glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
			glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
			glClear(GL_DEPTH_BUFFER_BIT);
			
			glUseProgram(shadowShader);
			// установка переменных в вершинном шейдере
			glUniformMatrix4fv(glGetUniformLocation(shadowShader, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));

			// rendering scene to depth map
			//glm::mat4 model = glm::mat4(1.0f); // единичная матрица (1 по диагонали)
			//model = glm::translate(model, glm::vec3(testChunk.posx, 0, testChunk.posz));
			//model = glm::rotate(model, 0.0f, glm::vec3(1.0, 0.0, 0.0));
			//model = glm::rotate(model, 0.0f, glm::vec3(0.0, 1.0, 0.0));
			//model = glm::rotate(model, 0.0f, glm::vec3(0.0, 0.0, 1.0));
			//model = glm::scale(model, glm::vec3(1,1,1));
			//glUniformMatrix4fv(glGetUniformLocation(shadowShader, "model"), 1, GL_FALSE, glm::value_ptr(model));

			for (size_t c = 0; c < chunksCount; c++)
			{
				Chunk& chunk = chunks[c];
				if (chunk.generated && !chunk.mesh.needUpdate) {
					glm::mat4 model = glm::mat4(1.0f); // единичная матрица (1 по диагонали)
					model = glm::translate(model, glm::vec3(0, 0, 0));
					model = glm::rotate(model, 0.0f, glm::vec3(1.0, 0.0, 0.0));
					model = glm::rotate(model, 0.0f, glm::vec3(0.0, 1.0, 0.0));
					model = glm::rotate(model, 0.0f, glm::vec3(0.0, 0.0, 1.0));
					model = glm::scale(model, glm::vec3(1,1,1));
					glUniformMatrix4fv(glGetUniformLocation(shadowShader, "model"), 1, GL_FALSE, glm::value_ptr(model));

					// render 1 chunk
					//glBindTexture(GL_TEXTURE_2D, textureAtlas);
					glUniform2i(glGetUniformLocation(shadowShader, "chunkPos"), chunk.posx, chunk.posz);
					glUniform2i(glGetUniformLocation(shadowShader, "atlasSize"), textureAtlas.width, textureAtlas.height);


					glBindVertexArray(chunk.mesh.VAO);
					glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, chunk.mesh.faceCount);

					glBindVertexArray(0);
				}
			}

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			//glCullFace(GL_BACK);
			glViewport(0, 0, display_w, display_h);
		}

		glm::vec3 skyColor = ambientColor * (sunDir.y + 1.0f) / 2.0f;
		glClearColor(skyColor.r, skyColor.g, skyColor.b, 255);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		if (wireframe_cb) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		else glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		// TEST
		//polyMeshUseShader(projection, view, lightSpaceMatrix);
		//for (size_t i = 0; i < ArraySize(testObjects); i++)
		//{
		//	TestPolyObject* o = &testObjects[i];
		//	polyMeshApplyTransform(o->pos, o->rot, o->scale);
		//	polyMeshDraw(*o->mesh, testTexture, depthMap, sunDir, sunColor, ambientColor);

		//}
		// TEST

		// draw sun and moon
		glDepthMask(GL_FALSE); // render on background
		useSpriteShader(projection, view);
		spriteApplyTransform(player.camera.pos + sunDir, 0.3, true);
		drawSprite(sunSprite, textureAtlas.ID);
		spriteApplyTransform(player.camera.pos + (sunDir * -1.0f), 0.3, true);
		drawSprite(moonSprite, textureAtlas.ID);
		glDepthMask(TRUE);

#define DEBUG_BLOCK 0
#define RENDER_CHUNKS 1
#if DEBUG_BLOCK
		//useCubesShader(sunDir, sunColor, moonColor, ambientColor, projection, view);
		//cubeApplyTransform(glm::vec3(0, 0, 0), glm::vec3(0, 0, 0), glm::vec3(1, 1, 1));
		//drawMesh(box, textureAtlas, { 0,0 }, { atlasWidth, atlasHeight });

		{
			useCubeShader(sunDir, sunColor, moonColor, ambientColor, projection, view, lightSpaceMatrix);
			cubeApplyTransform(glm::vec3(0, 0, 0), glm::vec3(0, 0, 0), glm::vec3(1, 1, 1));
			drawBlockMesh(testChunk.mesh, textureAtlas, depthMap, { 0,0 }, { atlasWidth, atlasHeight });
		}

#elif RENDER_CHUNKS
		// можно отрисовывать
		// draw chunks
		useCubeShader(sunDir, sunColor, moonColor, ambientColor, projection, view, lightSpaceMatrix);
		for (size_t c = 0; c < chunksCount; c++)
		{
			Chunk& chunk = chunks[c];
			if (chunk.generated && !chunk.mesh.needUpdate) {
				Block* blocks = chunk.blocks;
				cubeApplyTransform(glm::vec3(0, 0, 0), glm::vec3(0, 0, 0), glm::vec3(1, 1, 1));

				// render 1 chunk
				drawBlockMesh(
					chunks[c].mesh, textureAtlas, depthMap,
					glm::ivec2(chunks[c].posx, chunks[c].posz));
			}
		}
#endif	

#define DRAW_ENTITIES 0
#if DRAW_ENTITIES 
		// draw entities
		for (size_t i = 0; i < entities.count; i++)
		{
			cubeApplyTransform(entities.items[i].pos, entities.items[i].rot, {1,2,1});
			drawMesh(box, textureAtlas);
		}
#endif
		// draw debug geometry
		useFlatShader(projection, view);
		// chunk borders
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		if (debugView_cb) {
			for (size_t c = 0; c < chunksCount; c++)
			{
				Chunk& chunk = chunks[c];				
				flatApplyTransform(glm::vec3(chunk.posx, 0, chunk.posz), glm::vec3(0, 0, 0), glm::vec3(CHUNK_SX, CHUNK_SY, CHUNK_SZ));
				drawFlat(box, glm::vec3(0, 0, 0));
			}
		}

		Block* lookAtBlock;
		glm::vec3 lookAtBlockPos;
		static int maxDist = 10;



#define GRAVITY 0
#if GRAVITY
		// gravity, ground collision
		player.camera.pos.y -= 20 * deltaTime;
		Block* belowBlock;
		do {
			belowBlock = gameWorld.peekBlockFromPos(glm::vec3(player.camera.pos.x - chunk.posx, player.camera.pos.y - 2, player.camera.pos.z - chunk.posz));
			if (belowBlock && belowBlock->type != btAir)
				player.camera.pos.y = (int)player.camera.pos.y + 1;
			else
				break;
		} while (belowBlock);
#endif

		// destroying / building blocks
		lookAtBlock = gameWorld.peekBlockFromRay(player.camera.pos, player.camera.front, maxDist, &lookAtBlockPos);
		if (lookAtBlock) {
			flatApplyTransform(lookAtBlockPos, glm::vec3(0, 0, 0), glm::vec3(1.01, 1.01, 1.01));
			drawFlat(box, glm::vec3(0, 0, 0));

			if (gameInputs.attack) {
				lookAtBlock->type = btAir; // уничтожение блока
				updateLighting(*chunk);
				
				meshChunk(*chunk);
				updateBlockMesh(*chunkMesh);
			}

			if (gameInputs.placeBlock) {
				CubeSide side = BlockSideCastRay(player.camera.pos, player.camera.front, lookAtBlockPos);
				Block* placeBlock = 0;
				// TODO: проверка выхода за границы массива
				switch (side) {
				case cubeTop:		placeBlock = lookAtBlock + CHUNK_SX * CHUNK_SZ; dbgprint("top\n"); break;
				case cubeBottom:	placeBlock = lookAtBlock - CHUNK_SX * CHUNK_SZ; dbgprint("bottom\n"); break;
				case cubeFront:		placeBlock = lookAtBlock - CHUNK_SX;  dbgprint("front\n"); break;
				case cubeBack:		placeBlock = lookAtBlock + CHUNK_SX;  dbgprint("back\n"); break;
				case cubeRight:		placeBlock = lookAtBlock + 1;  dbgprint("right\n"); break;
				case cubeLeft:		placeBlock = lookAtBlock - 1;  dbgprint("left\n"); break;
				}

				if (placeBlock)
					placeBlock->type = btStone;

				
				// TODO: мы не знаем какой чанк нужно ремешить, так как мы меняли блок по ссылке
				updateLighting(*chunk);

				meshChunk(*chunk);
				updateBlockMesh(*chunkMesh);
			}
		}

		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		// axis
		useFlatShader(projection, view); // TODO: uniform переменные в шейдере можно установить один раз за кадр? Также можно испоьзовать uniform buffer
		flatApplyTransform(glm::vec3(0, 0, 0), glm::vec3(0, 0, 0), glm::vec3(1.5, 0.2, 0.2));
		drawFlat(box, glm::vec3(1, 0, 0));		
		flatApplyTransform(glm::vec3(0, 0, 0), glm::vec3(0, 0, 0), glm::vec3(0.2, 1.5, 0.2));
		drawFlat(box, glm::vec3(0, 1, 0));
		flatApplyTransform(glm::vec3(0, 0, 0), glm::vec3(0, 0, 0), glm::vec3(0.2, 0.2, 1.5));
		drawFlat(box, glm::vec3(0, 0, 1));
		
		// draw ui
		// TODO: верно устанавливать uv элементов интерфейса
		uiStart(window);
		uiDrawElement(uiAtlas.ID, glm::vec3(0, 0, 0), glm::vec3(64, 64, 1), uiUV[uiCross].scale, uiUV[uiCross].offset); // cursor
		
		//uiSetAnchor(uiTopAnchor, 360/2);
		//uiDrawElement(depthMap, { 0,0,0 }, { 360,360,1 }, { 1,1 }, { 0,0 }); // depthmap (shadow)

		
		uiSetAnchor(uiBottomAnchor, 50);
		uiShiftOrigin(-(8.0f * 70.0f / 2.0f), 0);
		for (size_t i = 0; i < 8; i++)
		{
			uiDrawElement(uiAtlas.ID, glm::vec3(0, 0, 0), glm::vec3(70, 70, 1), uiUV[uiInventoryCell].scale, uiUV[uiInventoryCell].offset); // inventory
			uiShiftOrigin(70, 0);
		}

		uiSetOrigin(-(35.0f * 8 / 2.0f), 0);
		uiSetAnchor(uiBottomAnchor, 110);
		for (size_t i = 0; i < 8; i++)
		{
			uiDrawElement(uiAtlas.ID, glm::vec3(0, 0, 0), glm::vec3(35, 35, 1), uiUV[uiHeart].scale, uiUV[uiHeart].offset); // health bar
			uiShiftOrigin(35, 0);
		}
		

#pragma endregion

		// ImGui
		GuiArgs guiArgs;
		guiArgs.col_mix_slider = &col_mix_slider;
		guiArgs.camspeed_slider = &player.speed;
		guiArgs.chunksUpdated = &chunksUpdated;
		guiArgs.wireframe_cb = &wireframe_cb;
		guiArgs.fov_slider = &fov_slider;
		guiArgs.debugView_cb = &debugView_cb;
		guiArgs.sunDir = &sunDir;
		guiArgs.sunColor = &sunColor;
		guiArgs.ambientColor = &ambientColor;
		guiArgs.currentChunkPos = glm::vec2(currentChunkPosX, currentChunkPosZ);
		cubes_gui(guiArgs);

		glfwSwapBuffers(window);
	}
}

static void cubes_gui(GuiArgs& args)
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	if (ImGui::Button("Rebuild shaders")) {
		initShaders();
	}
	ImGui::Separator();

	ImGui::SliderFloat("FOV", args.fov_slider, 0, 360);
	ImGui::SliderFloat("Cam speed", args.camspeed_slider, 1, 20);
	ImGui::Separator();
	ImGui::Checkbox("wireframe", args.wireframe_cb);
	ImGui::Checkbox("Debug view", args.debugView_cb);
	ImGui::Separator();
	ImGui::SliderFloat3("Sun position", (float*)args.sunDir, -1, 1);
	ImGui::ColorEdit3("Sun color", (float*)args.sunColor);
	ImGui::ColorEdit3("Ambient color", (float*)args.ambientColor);
	ImGui::Separator();
	ImGui::InputFloat3("Camera pos", (float*)&player.camera.pos);
	ImGui::InputFloat2("Chunk pos", (float*)&args.currentChunkPos);
	ImGui::InputFloat3("Camera front", (float*)&player.camera.front);
	ImGui::Separator();
	ImGui::SliderFloat("Shadow dimensions", &projDim, 0.1, 100);
	ImGui::SliderFloat("Shadow near plane", &near_plane, 0.1, 1000);
	ImGui::SliderFloat("Shadow far plane", &far_plane, 0.1, 1000);
	ImGui::SliderFloat("Shadow light dist", &shadowLightDist, 0, 50);

	ImGui::Separator();
	int taskCount = chunkGenQueue.taskCount;
	int completedCount = chunkGenQueue.taskCompletionCount;
	ImGui::InputInt("Chunk gen task count", &taskCount);
	ImGui::InputInt("Chunk gen task completed", &completedCount);



	static bool vsyncOn = false;
	if (vsyncOn) ImGui::Text("VSync ON");
	else ImGui::Text("VSync OFF");

	if (ImGui::Button("VSync")) {
		vsyncOn = !vsyncOn;
		typedef BOOL(APIENTRY* PFNWGLSWAPINTERVALPROC)(int);
		PFNWGLSWAPINTERVALPROC wglSwapIntervalEXT = 0;

		const char* extensions = (char*)glGetString(GL_EXTENSIONS);

		wglSwapIntervalEXT = (PFNWGLSWAPINTERVALPROC)wglGetProcAddress("wglSwapIntervalEXT");

		if (wglSwapIntervalEXT) {
			if (vsyncOn) wglSwapIntervalEXT(1);
			else wglSwapIntervalEXT(0);
		}
	}
	ImGui::Separator();
	if (ImGui::TreeNodeEx("Chunks")) {
		for (size_t i = 0; i < chunksCount; i++)
		{
			ImGui::PushID(i);
			int pos[2] = { chunks[i].posx, chunks[i].posz };
			ImGui::InputInt2("", pos);
			ImGui::PopID();
		}
		ImGui::TreePop();
	}

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}