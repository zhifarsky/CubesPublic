#pragma region external dependencies
#include <glm.hpp>
#include <gtc/noise.hpp>
#pragma endregion
#pragma region internal dependencies
#include "Cubes.h"
#include "Typedefs.h"
#include "Tools.h"
#include "Mesh.h"
#include "ui.h"
#include "Files.h"
#include "Entity.h"
#include "DataStructures.h"
#include "World.h"
#include "Input.h"
#include "Renderer.h"
#include "ChunkManager.h"
#include "Settings.h"
#include "RenderQueue.h"
#pragma endregion

// TDOO: перенести глобальные переменные

FrameBufferInfo* fbInfo;

bool g_DebugView = false;
bool g_RenderWireframe = false;
bool g_ShowDebugInfo = true;

bool g_UpdateSun = true;
bool g_GenChunks = true;

const int g_maxInteractionDistance = 12;

const glm::vec3 g_WorldUp(0, 1, 0);

struct SceneLighting {
	glm::vec3 directLightDirection;
	glm::vec3 directLightColor;
	glm::vec3 ambientLightColor;
};

struct {
	Geometry screenQuad, defaultQuad, defaultBox;
	Sprite sunSprite, moonSprite;
	FrameBuffer depthMapFBO;
	FrameBuffer screenFBO, intermediateFBO; // рендерим в screenFBO с MSAA, копируем результат в intermediateFBO и делаем постобработку
	Font* bigFont, *regularFont;
} Assets;

void InitFramebuffers(int width, int height, int samplesCount) {
	if ((width * height) == 0)
		return;

	Renderer::ReleaseFrameBuffer(&Assets.screenFBO);
	Renderer::ReleaseFrameBuffer(&Assets.intermediateFBO);

	Renderer::CreateMSAAFrameBuffer(&Assets.screenFBO, width, height, samplesCount);
	Renderer::CreateColorFrameBuffer(&Assets.intermediateFBO, width, height);
	Renderer::SetViewportDimensions(width, height);
}

void InitShadowMapBuffer(u32 size) {
	Renderer::ReleaseFrameBuffer(&Assets.depthMapFBO);
	Renderer::CreateDepthMapFrameBuffer(&Assets.depthMapFBO, size);
}

void RenderMainMenu(GameMemory* memory, GameState* gameState, Input* input);
void RenderPauseMenu(GameMemory* memory, GameState* gameState, Input* input);
void RenderGame(GameState* gameState, GameMemory* memory, Input* input);

void GameInit(GameState* gameState, GameMemory* memory) {
	dbgprint("GL VERSION %s\n", glGetString(GL_VERSION));
	
	UI::Init();

	// загрузка шрифтов
	Assets.regularFont = loadFont(memory, FONT_FOLDER "DigitalPixel.otf", 25);
	Assets.bigFont = loadFont(memory, FONT_FOLDER "DigitalPixel.otf", 50);

	{
		Vertex boxVerts[] = {
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
			boxVerts[i].normal = { 0,1,0 };
		}
		Triangle boxTris[] = {
			Triangle(0, 2, 1), Triangle(0, 3, 2),
			Triangle(4, 5, 7), Triangle(5, 6, 7),
			Triangle(0, 4, 3), Triangle(4, 7, 3), 
			Triangle(1, 2, 6), Triangle(1, 6, 5),
			Triangle(2, 3, 6), Triangle(3, 7, 6),
			Triangle(0, 1, 4), Triangle(1, 5, 4)
		};
		Assets.defaultBox = Renderer::CreateGeometry(boxVerts, 8, boxTris, 12);

		Vertex quadVerts[] = {
			Vertex(0,0,0, 0,0),
			Vertex(1,0,0, 1,0),
			Vertex(1,1,0, 1,1),
			Vertex(0,1,0, 0,1),
		};
		Triangle quadTris[] = {
			Triangle(0,1,2),
			Triangle(0,2,3)
		};
		Assets.defaultQuad = Renderer::CreateGeometry(quadVerts, 4, quadTris, 2);

		UV sunUV = GetUVFromAtlas(GetTexture(TextureAssetID::EnvTexture), tidSun, {16,16});
		UV moonUV = GetUVFromAtlas(GetTexture(TextureAssetID::EnvTexture), tidMoon, { 16,16 });
		Assets.sunSprite = createSprite(1, 1, sunUV);
		Assets.moonSprite = createSprite(1, 1, moonUV);
	}

	{
		Vertex vertices[] = {
			Vertex(-1,-1, 0, 0,0),
			Vertex(1,-1, 0, 1,0),
			Vertex(1, 1, 0, 1,1),
			Vertex(-1, 1, 0, 0,1),
		};
		Triangle triangles[] = {
			Triangle(0,1,2),
			Triangle(0,2,3)
		};

		Assets.screenQuad = Renderer::CreateGeometry(vertices, 4, triangles, 2);
	}

	{
		Entity entity;
		entity.type = entityZombie;
		entity.state = entityStateIdle;
		entity.pos = { 0, CHUNK_SY - 2, 0 };
		gameState->entities.append(entity);

		entity.type = entityZombie;
		entity.state = entityStateIdle;
		entity.pos = { 5, CHUNK_SY - 2, 5 };
		gameState->entities.append(entity);
		
		entity.pos = { -15, CHUNK_SY - 2, -15 };
		gameState->entities.append(entity);
	}

	{
		// shadow framebuffer
		InitShadowMapBuffer(pow(2, gameState->shadowQuality));
	}
}

void GameExit() {
	ChunksSaveToDisk(g_chunkManager.chunks, g_chunkManager.chunksCount, g_gameWorld.info.name);
	CloseWindow();
}

// time - время в секундах
void GameUpdateAndRender(GameMemory* memory, float time, Input* input, FrameBufferInfo* frameBufferInfo) {
	Renderer::Begin(&memory->tempStorage);
	
	//
	// memory initialisaiton
	//

	if (!memory->isInitialized) {
		GameState* gameState = ArenaPushStruct(&memory->permStorage, GameState);

		gameState->time = time;


		Settings settings;
		SettingsLoad(&settings);
		gameState->shadowQuality = settings.shadowQuality;
		gameState->AASamplesCount = settings.antiAliasingQuality;
		SetVsync(settings.vsync);

		gameState->entities.alloc(&memory->permStorage, 1024);
		gameState->droppedItems.alloc(&memory->permStorage, 1024);

		gameState->nearPlane = 0.1;
		gameState->farPlane = 1000;
		
		gameState->shadowRenderDistance = 128;
		gameState->shadowLightDistance = 128;

		GameInit(gameState, memory);
		gameState->status = gsMainMenu;

		memory->isInitialized = true;
	}

	GameState* gameState = (GameState*)memory->permStorage.mem;

	gameState->deltaTime = time - gameState->time; // текущее время - предыдущее время
	gameState->time = time;

	fbInfo = frameBufferInfo;
	static FrameBufferInfo lastFBInfo = { 0 };
	if (lastFBInfo.sizeX != frameBufferInfo->sizeX || lastFBInfo.sizeY != frameBufferInfo->sizeY) {
		InitFramebuffers(frameBufferInfo->sizeX, frameBufferInfo->sizeY, gameState->AASamplesCount);
	}

	//if (ButtonClicked(input->startGame)) {
	//	if (gameState->status == gsMainMenu)
	//		gameState->status = gsInGame;
	//}
	if (ButtonClicked(input->switchExitMenu)) {
		if (gameState->status == gsInGame)
			gameState->status = gsExitMenu;
		else if (gameState->status == gsExitMenu) {
			// save settings to file
			{
				Settings settings;
				settings.FOV = g_gameWorld.player.camera.FOV;
				settings.shadowQuality = gameState->shadowQuality;
				settings.renderDistance = GetRenderDistance(g_chunkManager.chunksCount);
				settings.antiAliasingQuality = gameState->AASamplesCount;
				settings.vsync = GetVsync();
				SettingsSave(&settings);
			}

			gameState->status = gsInGame;
		}
	}
	if (ButtonClicked(input->switchFullscreenMode)) {
		switch (WindowGetCurrentMode())
		{
		case WindowMode::Windowed:
			WindowSwitchMode(WindowMode::WindowedFullScreen);
			break;
		case WindowMode::WindowedFullScreen:
			WindowSwitchMode(WindowMode::Windowed);
			break;
		}
	}
	if (ButtonClicked(input->rebuildShaders)) {
		dbgprint("Rebuilding shaders...\n");
		InvalidateShaders();
		dbgprint("Shaders rebuild done!\n");
	}
	if (ButtonClicked(input->showDebugInfo)) {
		g_ShowDebugInfo = !g_ShowDebugInfo;
	}

	switch (gameState->status)
	{
	case gsMainMenu:
		SetCursorMode(true);
		RenderMainMenu(memory, gameState, input);
		break;
	case gsExitMenu:
		SetCursorMode(true);
		RenderPauseMenu(memory, gameState, input);
		break;
	case gsInGame:
		RenderGame(gameState, memory, input);
		break;
	}

	// в полноэкранном окне не отрисовывается системный курсор, поэтому отрисовываем его самостоятельно 
	if (gameState->status == gsMainMenu ||
		gameState->status == gsExitMenu)
	{
		UI::Begin(&memory->tempStorage, input, Assets.regularFont, fbInfo);
		double xpos = 0, ypos = 0;
		GetCursorPos(&xpos, &ypos);
		UI::SetOrigin(xpos, fbInfo->sizeY - ypos);
		UI::DrawElement(GetTexture(TextureAssetID::UITexture), glm::vec3(0, 0, 0), glm::vec3(64, 64, 1), uiCross, {16, 16});
		UI::End();
	}

  lastFBInfo = *frameBufferInfo;
}

void RenderMainMenu(GameMemory* memory, GameState* gameState, Input* input) {
	enum class MainMenuStatus {
		Main,
		SelectWorld
	};
	static MainMenuStatus menuStatus = MainMenuStatus::Main;

	Renderer::Clear(0.6, 0.6, 0.6);

	UI::Begin(&memory->tempStorage, input, Assets.regularFont, fbInfo);
	UI::SetAnchor(uiAnchor::Center, 0);
	UI::SetAdvanceMode(AdvanceMode::Down);
	
	switch (menuStatus)
	{
	case MainMenuStatus::Main:
	{
		if (UI::Button("Start game", {0,0}, true)) {
			menuStatus = MainMenuStatus::SelectWorld;
		}
	} break;
	case MainMenuStatus::SelectWorld:
	{
		static DynamicArray<GameWorldInfo> worldsList = {0};
		static bool updateWorldsList = true;
		if (updateWorldsList) {
			updateWorldsList = false;
			worldsList.clear();
			EnumerateWorlds(&worldsList);
		}
		if (UI::Button("Update", {0,0}, true)) {
			updateWorldsList = true;
		}

		auto StartGame = [](GameMemory* memory, GameState* gameState, GameWorldInfo* worldInfo) {
			Settings settings;
			SettingsLoad(&settings);

			Player& player = g_gameWorld.player;
			player.camera.pos = glm::vec3(8, 30, 8);
			player.camera.front = glm::vec3(0, 0, -1);
			player.camera.up = g_WorldUp;
			player.camera.FOV = settings.FOV;
			player.maxSpeed = 15;
			player.inventory = InventoryCreate();
#if _DEBUG
			InventoryAddItem(&player.inventory, ItemType::GroundBlock, 64);
			InventoryAddItem(&player.inventory, ItemType::StoneBlock, 64);
			InventoryAddItem(&player.inventory, ItemType::Sword, 64);
			InventoryAddItem(&player.inventory, ItemType::Pickaxe, 64);
#endif

			g_gameWorld.init(memory, worldInfo, settings.renderDistance);

			gameState->status = gsInGame;
		};
		
		// load existing world
		for (size_t i = 0; i < worldsList.count; i++)
		{
			if (UI::Button(worldsList[i].name, {0,0}, true)) {
				StartGame(memory, gameState, &worldsList.items[i]);
			}
		}

		// create new world
		if (UI::Button("Start new world", {0,0}, true)) {
			GameWorldInfo worldInfo;
			worldInfo.seed = 0;
			
			// find name for new world
			{
				char worldPath[256];
				const char *newWorldNameBase = "New World";

				int i = 1;
				do {
					sprintf(worldInfo.name, "%s %d", newWorldNameBase, i);
					GetWorldPath(worldPath, worldInfo.name);
					i++;
				} while (IsFileExists(worldPath));
			}

			StartGame(memory, gameState, &worldInfo);
		}
	} break;
	}

	UI::SetAnchor(uiAnchor::Top, 100);
	UI::UseFont(Assets.bigFont);
	UI::Text("Main menu", true);
	UI::End();
}

void RenderPauseMenu(GameMemory* memory, GameState* gameState, Input* input) {
	enum class PauseMenuState {
		MainMenu,
		SettingsMenu
	};
	static PauseMenuState menuState = PauseMenuState::MainMenu;

	Renderer::Clear(0.6, 0.6, 0.6);

	UI::Begin(&memory->tempStorage, input, Assets.regularFont, fbInfo);
	UI::SetAdvanceMode(AdvanceMode::Down);
	float elemWidth = 400;

	switch (menuState)
	{
	case PauseMenuState::MainMenu: {
		UI::SetAnchor(uiAnchor::Center, 0);
		if (UI::Button("Return", glm::vec2(elemWidth, 0), true)) {
			gameState->status = gsInGame;
		}
		if (UI::Button("Settings", glm::vec2(elemWidth, 0), true)) {
			menuState = PauseMenuState::SettingsMenu;
		}
		if (UI::Button("Exit", glm::vec2(elemWidth, 0), true)) {
			GameExit();
		}
	} break;
	case PauseMenuState::SettingsMenu: {
		UI::SetAnchor(uiAnchor::Center, 0);
		UI::ShiftOrigin(-elemWidth / 2, 400);

		bool fullScreen = (WindowGetCurrentMode() == WindowMode::WindowedFullScreen);
		if (UI::CheckBox("Fullscreen mode", &fullScreen)) {
			if (fullScreen)
				WindowSwitchMode(WindowMode::WindowedFullScreen);
			else
				WindowSwitchMode(WindowMode::Windowed);
		}

		UI::SliderFloat("FOV", &g_gameWorld.player.camera.FOV, 40, 120, elemWidth);

		int renderDistanceSlider = GetRenderDistance(g_chunkManager.chunksCount);
		if (UI::SliderInt("Render distance", &renderDistanceSlider, MIN_RENDER_DISTANCE, MAX_RENDER_DISTANCE, elemWidth)) {
			if (GetRenderDistance(g_chunkManager.chunksCount) != renderDistanceSlider) {
				ChunkManagerReleaseChunks(memory, &g_chunkManager);
				ChunkManagerAllocChunks(memory, &g_chunkManager, renderDistanceSlider);
			}
		}

		if (UI::SliderInt("Shadow quality", &gameState->shadowQuality, 10, 14, elemWidth)) {
			InitShadowMapBuffer(pow(2, gameState->shadowQuality));
		}

		if (UI::SliderInt("Anti Aliasing quality", &gameState->AASamplesCount, 1, Renderer::GetMaxAASamples(), elemWidth)) {
			InitFramebuffers(fbInfo->sizeX, fbInfo->sizeY, gameState->AASamplesCount);
		}

		UI::CheckBox("Update sun", &g_UpdateSun);
		if (!g_UpdateSun) {
			static float sunSlider = 0;
			UI::SliderFloat("Sun direction", &sunSlider, -2, 2);
			gameState->sunDirection.x = cos(sunSlider);
			gameState->sunDirection.y = sin(sunSlider);
			gameState->sunDirection.z = 0;
		}

		UI::CheckBox("Wireframe mode", &g_RenderWireframe);
		
		UI::CheckBox("Debug view", &g_DebugView);
		UI::CheckBox("Debug info", &g_ShowDebugInfo);
		UI::CheckBox("Generate chunks", &g_GenChunks);

		bool vsyncOn = GetVsync();
		if (UI::CheckBox("Vsync", &vsyncOn)) {
			SetVsync(vsyncOn);
		}

		if (UI::Button("Back", glm::vec2(elemWidth, 0))) {
			menuState = PauseMenuState::MainMenu;
		}
	} break;
	default:
		break;
	}

	UI::End();
}

void RenderGame(GameState* gameState, GameMemory* memory, Input* input) {
	Player& player = g_gameWorld.player;

	if (gameState->inventoryOpened)
		SetCursorMode(true);
	else
		SetCursorMode(false);

	// update player rotation
	if (!gameState->inventoryOpened)
	{
		static float lastX = 0, lastY = 0; // позиция курсора
		static float yaw = 0, pitch = 0;
		static const float sensitivity = 0.1f; // TODO: перенести в настройки

		double xpos, ypos;
		GetCursorPos(&xpos, &ypos);

		float xoffset = xpos - lastX;
		float yoffset = lastY - ypos; // reversed since y-coordinates range from bottom to top
		lastX = xpos;
		lastY = ypos;

		xoffset *= sensitivity;
		yoffset *= sensitivity;

		yaw += xoffset;
		pitch += yoffset;

		if (pitch > 89.0f)
			pitch = 89.0f;
		if (pitch < -89.0f)
			pitch = -89.0f;

		// update player camera
		glm::vec3 direction;
		direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
		direction.y = sin(glm::radians(pitch));
		direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
		player.camera.front = glm::normalize(direction);
		
		glm::vec3 right = -glm::cross(player.camera.front, g_WorldUp);
		player.camera.up = glm::cross(player.camera.front, right);
	}

	// update player pos
	{
		float cameraSpeedAdj = player.maxSpeed * gameState->deltaTime; // делаем скорость камеры независимой от FPS
		float jumpSpeedAdj = 100 * gameState->deltaTime;
		static bool flyMode = true;
		if (flyMode) {
			if (ButtonHeldDown(input->forward))
				player.speedVector += cameraSpeedAdj * glm::normalize(player.camera.front * glm::vec3(1, 0, 1));
			if (ButtonHeldDown(input->backwards))
				player.speedVector -= cameraSpeedAdj * glm::normalize(player.camera.front * glm::vec3(1, 0, 1));
			if (ButtonHeldDown(input->left))
				player.speedVector -= glm::normalize(glm::cross(glm::normalize(player.camera.front * glm::vec3(1, 0, 1)), player.camera.up)) * cameraSpeedAdj;;
			if (ButtonHeldDown(input->right))
				player.speedVector += glm::normalize(glm::cross(glm::normalize(player.camera.front * glm::vec3(1, 0, 1)), player.camera.up)) * cameraSpeedAdj;

			if (ButtonHeldDown(input->up))
				player.speedVector += cameraSpeedAdj * g_WorldUp;
			if (ButtonHeldDown(input->down))
				player.speedVector -= cameraSpeedAdj * g_WorldUp;
		}
		else {
			if (ButtonHeldDown(input->forward))
				player.speedVector += cameraSpeedAdj * (player.camera.front * glm::vec3(1, 0, 1));
			if (ButtonHeldDown(input->backwards))
				player.speedVector -= cameraSpeedAdj * (player.camera.front * glm::vec3(1, 0, 1));
			if (ButtonHeldDown(input->left))
				player.speedVector -= glm::normalize(glm::cross((player.camera.front * glm::vec3(1,0,1)), player.camera.up)) * cameraSpeedAdj;;
			if (ButtonHeldDown(input->right))
				player.speedVector += glm::normalize(glm::cross((player.camera.front * glm::vec3(1, 0, 1)), player.camera.up)) * cameraSpeedAdj;
				
			if (ButtonClicked(input->up))
				player.speedVector += jumpSpeedAdj * player.camera.up;
		}

		glm::vec3 prevPos = player.camera.pos;
		player.camera.pos += player.speedVector;
		player.speedVector *= 0.8; // TODO: deltatime

#if 0
		// gravity, ground collision
		player.camera.pos.y -= 20 * gameState->deltaTime; // gravity
		Block *block1, *block2;
		//do {
		//	belowBlock = ChunkManagerPeekBlockFromPos(&g_chunkManager, player.camera.pos.x, player.camera.pos.y - 2, player.camera.pos.z);
		//	if (belowBlock && belowBlock->type != BlockType::btAir)
		//		player.camera.pos.y = (int)player.camera.pos.y + 1;
		//	else
		//		break;
		//} while (belowBlock);
		block1 = ChunkManagerPeekBlockFromPos(&g_chunkManager, player.camera.pos.x, player.camera.pos.y - 2, player.camera.pos.z);
		block2 = ChunkManagerPeekBlockFromPos(&g_chunkManager, player.camera.pos.x, player.camera.pos.y - 1, player.camera.pos.z);
			
		if ((block1 && block1->type != BlockType::btAir) ||
			(block2 && block2->type != BlockType::btAir)) 
		{
			player.camera.pos = { prevPos.x, floor(prevPos.y) + 0.01, prevPos.z };
		}
#endif
	}

	// player inventory
	{
		if (ButtonClicked(input->openInventory)) {
			gameState->inventoryOpened = !gameState->inventoryOpened;
		}

		for (size_t i = 0; i < INVENTORY_MAX_SIZE; i++)
		{
			if (ButtonClicked(input->inventorySlots[i])) {
				InventorySelectItem(&player.inventory, i);
			}
		}

		if (ButtonClicked(input->scrollUp)) {
			InventorySelectItem(&player.inventory, player.inventory.selectedIndex - 1);
		}
		if (ButtonClicked(input->scrollDown)) {
			InventorySelectItem(&player.inventory, player.inventory.selectedIndex + 1);
		}
	}

	// collect dropped items
	float collectRadius = 3;
	for (size_t i = 0; i < gameState->droppedItems.count; i++)
	{
		Item* item = &gameState->droppedItems[i];
		if (item->count < 1)
			continue;

		if (glm::distance(player.camera.pos, item->pos) < collectRadius) {
			InventoryAddItem(&player.inventory, item->type, item->count);
			item->count = 0; 
		}
	}

	// update sun and moon

	SceneLighting lighting;
	glm::vec3 moonDir;
	{
		float sunSpeed = 0.05;
		glm::vec3 sunColor(1, 0.9, 0.8);
		glm::vec3 moonColor(0.6, 0.6, 0.9);
		glm::vec3 skyColor(0.4, 0.4, 0.8);
		
		if (g_UpdateSun) {
			gameState->sunDirection.x = cos((gameState->time + 10) * sunSpeed);
			gameState->sunDirection.y = sin((gameState->time + 10) * sunSpeed);
			gameState->sunDirection.z = 0;
		}
	
		moonDir = -gameState->sunDirection;

		// если день
		if (gameState->sunDirection.y > 0) {
			float sunIntencity = glm::max(gameState->sunDirection.y, 0.0f);
			lighting.ambientLightColor = skyColor * glm::max(sunIntencity, 0.2f);
			lighting.directLightColor = sunColor * sunIntencity;

			lighting.directLightDirection = gameState->sunDirection;
		}
		// если ночь
		else {
			float moonIntencity = glm::max(moonDir.y, 0.0f) * 0.3;
			lighting.directLightColor = moonColor * moonIntencity;
			lighting.ambientLightColor = skyColor * glm::max(moonIntencity, 0.2f);

			lighting.directLightDirection = moonDir;
		}
	}

	Frustum frustum = FrustumCreate(
		player.camera.pos, player.camera.front, player.camera.up,
		(float)fbInfo->sizeX / fbInfo->sizeY,
		player.camera.FOV,
		gameState->nearPlane, gameState->farPlane);

	int currentChunkPosX = (int)(player.camera.pos.x / CHUNK_SX) * CHUNK_SX;
	int currentChunkPosZ = (int)(player.camera.pos.z / CHUNK_SZ) * CHUNK_SZ;
	if (player.camera.pos.x < 0)
		currentChunkPosX -= CHUNK_SX;
	if (player.camera.pos.z < 0)
		currentChunkPosZ -= CHUNK_SZ;

	if (g_GenChunks) {
		if (ButtonClicked(input->regenerateChunks)) {
			//g_chunkManager.queue->StopAndJoin();
			//for (size_t i = 0; i < g_chunkManager.chunksCount; i++)
			//{
			//	g_chunkManager.chunks[i].status = ChunkStatus::Uninitalized;
			//}
			//g_chunkManager.queue->Start(std::max(1, GetThreadsCount()));
		}

		ChunkManagerBuildChunks(&g_chunkManager, &memory->tempStorage, &frustum, player.camera.pos.x, player.camera.pos.z);
	}

	//
	// building / destruction
	//

	// place block
	if (ButtonClicked(input->placeBlock)) {
		InventoryCell *cell = &player.inventory.cells[player.inventory.selectedIndex];
		if (cell->itemsCount > 0) {
			BlockType blockType = GetItemInfo(cell->itemType)->blockType;
			auto res = ChunkManagerPlaceBlock(&memory->tempStorage, &g_chunkManager, blockType, player.camera.pos, player.camera.front, g_maxInteractionDistance);
			if (res.success) {
				InventoryDropItem(&player.inventory, player.inventory.selectedIndex, 1);
			}
		}
	}
	// destroy block
	else if (ButtonHeldDown(input->attack)) {
		{
			PeekBlockResult res = ChunkManagerPeekBlockFromRay(&g_chunkManager, player.camera.pos, player.camera.front, g_maxInteractionDistance);
			if (res.success) {
				static glm::ivec3 lastBlokPos(0);
				static float startTime = gameState->time;
				glm::ivec3 blockPos = res.blockPos;
				if (blockPos == lastBlokPos) {
					float blockHardness = GetBlockInfo(res.block->type)->hardness;
					float itemDamage = GetItemInfo(InventoryGetCurrentItem(&player.inventory).itemType)->blockDamage;
					float timeToDestroyBlock = blockHardness / itemDamage;

					if (gameState->time - startTime > timeToDestroyBlock) {
						PlaceBlockResult res = ChunkManagerDestroyBlock(&memory->tempStorage, &g_chunkManager, player.camera.pos, player.camera.front, g_maxInteractionDistance);
						if (res.success) {
							Item drop = { 
								.pos = glm::vec3(res.pos.x + 0.5, res.pos.y + 0.5, res.pos.z + 0.5), 
								.count = 1, 
								.type = GetBlockInfo(res.typePrev)->itemType 
							};
							gameState->droppedItems.append(drop);

						}
						startTime = gameState->time;
					}
				}
				else {
					startTime = gameState->time;
				}
				lastBlokPos = blockPos;
			}
		}
	}

	// update entities
	for (size_t i = 0; i < gameState->entities.count; i++)
	{
		Entity& entity = gameState->entities[i];

		if (entity.type == entityNull)
			continue;

		// gravity
		entity.pos.y -= 5 * gameState->deltaTime;
		entity.pos += entity.speed * gameState->deltaTime;
		entity.speed *= 0.1;
		Block* belowBlock;
#if 1
		do {
			belowBlock = ChunkManagerPeekBlockFromPos(&g_chunkManager, entity.pos.x, entity.pos.y, entity.pos.z);
			if (belowBlock && belowBlock->type != BlockType::btAir)
				entity.pos.y = (int)entity.pos.y + 1;
			else
				break;
		} while (belowBlock);
#endif

		if (glm::distance(entity.pos, player.camera.pos) < 16) {
			entity.state = entityStateChasing;
		}
		else {
			entity.state = entityStateIdle;
		}

		switch (gameState->entities[i].state) {
		case entityStateChasing:
			glm::vec3 toPlayer = (player.camera.pos - entity.pos) * glm::vec3(1, 0, 1);
			if (glm::length(toPlayer) > 0) {
				gameState->entities[i].pos += glm::normalize(toPlayer) * 3.0f * gameState->deltaTime;

				glm::vec3 dir = glm::normalize(toPlayer);
				float yaw = atan2f(dir.x, dir.z);
				float pitch = asinf(-dir.y);

				gameState->entities[i].rot = glm::degrees(glm::vec3(pitch, yaw, 0.0f));
			}
			break;
		case entityStateIdle:
			break;
		}
	}

#pragma region Rendering
	
	// clear framebuffers & bind Screen Framebuffer
	{
		Renderer::BindFrameBuffer(&Assets.depthMapFBO);
		Renderer::SetViewportDimensions(Assets.depthMapFBO.textures[0].width, Assets.depthMapFBO.textures[0].height);
		Renderer::Clear(0, 0, 0);
		
		Renderer::BindFrameBuffer(&Assets.screenFBO);
		Renderer::SetViewportDimensions(fbInfo->sizeX, fbInfo->sizeY);
		Renderer::Clear(lighting.ambientLightColor.r, lighting.ambientLightColor.g, lighting.ambientLightColor.b);
	}

	// трансформация вида (камера)
	glm::mat4 view, projection;
	{
		view = glm::lookAt(player.camera.pos, player.camera.pos + player.camera.front, player.camera.up);

		// матрица проекции (перспективная/ортогональная проекция)
		projection = glm::mat4(1);
		projection = glm::perspective(glm::radians(player.camera.FOV), (float)fbInfo->sizeX / (float)fbInfo->sizeY, gameState->nearPlane, gameState->farPlane);
	}

	// трансформация тени
	glm::mat4 lightSpaceMatrix;
	{
		glm::mat4 lightProjection = glm::ortho(
			-gameState->shadowRenderDistance,
			gameState->shadowRenderDistance, 
			-gameState->shadowRenderDistance, 
			gameState->shadowRenderDistance, 
			gameState->nearPlane, gameState->farPlane);
		
		glm::mat4 lightView = glm::lookAt(
			gameState->shadowLightDistance * lighting.directLightDirection + player.camera.pos * glm::vec3(1, 0, 1),
			player.camera.pos * glm::vec3(1, 0, 1),
			glm::vec3(0.0f, 1.0f, 0.0f));

		lightSpaceMatrix = lightProjection * lightView;
	}

	//
	// Get Shaders
	//

	Shader polymeshShader = GetShader(ShaderAssetID::PolymeshShader);
	Shader polymeshShadowShader = GetShader(ShaderAssetID::PolymeshShadowShader);
	Shader cubeShader = GetShader(ShaderAssetID::CubeShader);
	Shader cubeShadowShader = GetShader(ShaderAssetID::CubeShadowShader);
	Shader spriteShader = GetShader(ShaderAssetID::SpriteShader);

	//
	// Init Render Queue
	//

	RenderQueue renderQueue;
	u64 renderQueueSize = Megabytes(5);
	RenderQueueInit(&renderQueue, ArenaPush(&memory->tempStorage, renderQueueSize), renderQueueSize, &Assets.screenFBO, &Assets.depthMapFBO);
	{
		RenderEntryCamera cameraEntry = { .frustum = frustum, .view = view, .projection = projection };
		RenderQueuePush(&renderQueue, &cameraEntry);
	}
	{
		RenderEntryLighting lightingEntry = {
			.lightSpaceMatrix = lightSpaceMatrix,
			.directLightDirection = lighting.directLightDirection,
			.directLightColor = lighting.directLightColor,
			.ambientLightColor = lighting.ambientLightColor
		};
		RenderQueuePush(&renderQueue, &lightingEntry);
	}

	// TEST
	{
		float epsilon = 1e-6;

		auto RayIntersection = [](glm::vec3& rayOrigin, glm::vec3& rayDir, u32 plane, float planeOffset) -> glm::vec3 {
			float epsilon = 1e-6;
			if (plane > 2 ||
				abs(rayDir[plane]) < epsilon)
			{
				return { 0,0,0 };
			}

			float t = -(rayOrigin[plane] - planeOffset) / rayDir[plane];
			glm::vec3 res = rayOrigin + (t * rayDir);
			res[plane] = planeOffset;

			return res;
			};

		glm::vec3& rayDir = player.camera.front;
		glm::vec3& rayOrigin = player.camera.pos;

		for (size_t j = 0; j < 2; j++)
		{
			for (size_t i = 0; i < 3; i++)
			{
				glm::vec3 p = RayIntersection(rayOrigin, rayDir, i, j);
				if (p.length() > 0) {
					if (p.x >= 0 && p.x <= 1 &&
						p.y >= 0 && p.y <= 1 &&
						p.z >= 0 && p.z <= 1)
					{
						glm::vec3 color(0.0f);
						color[i] = 1;
						
						// draw
						{
							glm::mat4 model(1);
							model = glm::translate(model, p);
							model = glm::scale(model, { 0.4, 0.4, 0.4 });
							model = glm::translate(model, { -.5,-.5,-.5 });
							
							RenderEntryTexturedMesh entry = MakeRenderEntryTexturedMesh(
								Assets.defaultBox.VAO, Assets.defaultBox.triangleCount, 
								model, polymeshShader, 
								0, 0, 
								true, color);
							RenderQueuePush(&renderQueue, &entry);
						}
					}
				}
			}
		}

		// cube
		if (false)
		{
			glm::mat4 model(1);
			model = glm::translate(model, { 0,0,0 });
			model = glm::scale(model, { 1, 1, 1 });

			RenderEntryTexturedMesh entry = MakeRenderEntryTexturedMesh(Assets.defaultBox.VAO, Assets.defaultBox.triangleCount, model, polymeshShader, 0, 0, true, { 0.5, 0.5, 0.5 });
			RenderQueuePush(&renderQueue, &entry);
		}

		//player.camera.pos.x = 0;
		//player.camera.pos.y = 0;

		Renderer::UnbindShader();
	}
	// TEST

	// draw chunks & entities
	u32 renderCommandsExecuted = 0;
	{
		// draw shadows
		//Renderer::BindFrameBuffer(&Assets.depthMapFBO);
		//Renderer::SetViewportDimensions(Assets.depthMapFBO.textures[0].width, Assets.depthMapFBO.textures[0].height);
		//DrawChunksShadow(g_chunkManager.chunks, g_chunkManager.chunksCount, &Assets.depthMapFBO, &frustum, lightSpaceMatrix);
		//DrawEntitiesShadow(gameState->entities.items, gameState->entities.count, &Assets.depthMapFBO, lightSpaceMatrix);

		Renderer::BindFrameBuffer(&Assets.screenFBO);
		Renderer::SetViewportDimensions(fbInfo->sizeX, fbInfo->sizeY);
		
		// draw entities
		{
			Geometry* mesh = GetMesh(MeshAssetID::EntityMesh);
			Texture* texture = GetTexture(TextureAssetID::EntityTexture);

			for (size_t i = 0; i < gameState->entities.count; i++)
			{
				Entity* e = &gameState->entities[i];

				// TODO: frustum culling

				// shadow pass
				RenderEntryMeshShadow entryShadow = MakeRenderEntryMeshShadow(
					mesh->VAO, mesh->triangleCount, 
					GetTransform(e->pos, e->rot), polymeshShadowShader);

				// main pass
				RenderEntryTexturedMesh entryMain = MakeRenderEntryTexturedMesh(
					mesh->VAO, mesh->triangleCount,
					GetTransform(e->pos, e->rot), polymeshShader,
					texture, &Assets.depthMapFBO.textures[0]);

				RenderQueuePush(&renderQueue, &entryShadow);
				RenderQueuePush(&renderQueue, &entryMain);
			}
		}

		// draw chunks
		{
			Texture* texture = GetTexture(TextureAssetID::EnvTexture);

			for (size_t i = 0; i < g_chunkManager.chunksCount; i++)
			{
				Chunk* chunk = &g_chunkManager.chunks[i];

				if (!chunk->generationInProgress && chunk->status == ChunkStatus::ReadyToRender) {
					glm::vec3 chunkCenter(chunk->posx + CHUNK_SX / 2.0f, CHUNK_SY / 2.0f, chunk->posz + CHUNK_SZ / 2.0f);
					if (FrustumSphereIntersection(&frustum, chunkCenter, g_chunkRadius)) {
						glm::mat4 transform = GetTransform({ chunk->posx, 0, chunk->posz });

						// shadow pass
						RenderEntryMeshShadowInstanced entryShadow = MakeRenderEntryMeshShadowInstanced(
							chunk->mesh.VAO, 2, chunk->mesh.instanceCount, 
							transform, 
							cubeShadowShader);
						RenderQueuePush(&renderQueue, &entryShadow);

						// main pass
						RenderEntryTexturedMeshInstanced entryMain = MakeRenderEntryTexturedMeshInstanced(
							chunk->mesh.VAO, 2, chunk->mesh.instanceCount,
							transform, 
							cubeShader, texture, &Assets.depthMapFBO.textures[0]);
						RenderQueuePush(&renderQueue, &entryMain);

						// draw wireframe
						if (g_RenderWireframe) {
							RenderEntryTexturedMeshInstanced entryWireframe = MakeRenderEntryTexturedMeshInstanced(
								chunk->mesh.VAO, 2, chunk->mesh.instanceCount,
								transform,
								cubeShader, texture, &Assets.depthMapFBO.textures[0],
								true, { 0,0,0 }, true);

							RenderQueuePush(&renderQueue, &entryWireframe);
						}

						// chunk bounds
						if (g_DebugView) {
							glm::mat4 model = GetTransform({ chunk->posx, 0, chunk->posz }, { 0,0,0 }, { CHUNK_SX, CHUNK_SY, CHUNK_SZ });
							RenderEntryTexturedMesh entryChunkBounds = 
								MakeRenderEntryTexturedMesh(
									Assets.defaultBox.VAO, Assets.defaultBox.triangleCount, 
									model, 
									polymeshShader, 
									0, 0, 
									true, { 0,0,0 }, true);
							
							RenderQueuePush(&renderQueue, &entryChunkBounds);
						}
					}
				}
#if _DEBUG
				// draw debug info
				else {
					glm::mat4 model = GetTransform({ chunk->posx, 0, chunk->posz }, glm::vec3(0), { CHUNK_SX, CHUNK_SY, CHUNK_SZ });
					glm::vec3 color(0);

					if (chunk->status == ChunkStatus::ReadyToRender) {
						color = { 1,0,0 };
					}
					else if (chunk->status == ChunkStatus::Generated) {
						color = { 0,1,0 };
					}
					else if (chunk->status == ChunkStatus::Initialized) {
						color = { 0,0,0 };
					}

					RenderEntryTexturedMesh entry = MakeRenderEntryTexturedMesh(
						Assets.defaultBox.VAO, Assets.defaultBox.triangleCount,
						model, polymeshShader, 
						0, 0, 
						true, color);

					RenderQueuePush(&renderQueue, &entry);
				}
#endif
			}
		}
	}

	// draw sun and moon
	{
		// sun
		RenderEntrySprite entry = {};
		entry.shader = spriteShader;
		entry.texture = GetTexture(TextureAssetID::EnvTexture);
		entry.VAO = Assets.sunSprite.VAO;
		entry.triangleCount = 2;
		entry.scale = 0.3;
		entry.spherical = true;
		entry.drawOnBackground = true;
		entry.transform = GetTransform(player.camera.pos + gameState->sunDirection);

		RenderQueuePush(&renderQueue, &entry);

		// moon
		entry.VAO = Assets.moonSprite.VAO;
		entry.transform = GetTransform(player.camera.pos + moonDir);
		RenderQueuePush(&renderQueue, &entry);
	}


	// draw player hand
	{
		Texture* texture = GetTexture(TextureAssetID::UITexture);
		glm::vec3 defaultFront(0, 0, -1);
		glm::vec3 front = glm::normalize(player.camera.front);
		glm::vec3 right = glm::normalize(glm::cross(front, g_WorldUp));

		//glm::vec3 eye = player.camera.pos + front * 1.5f + right * 2.0f;
		//glm::vec3 center = player.camera.pos;
		glm::vec3 eye = player.camera.pos + front;
		glm::vec3 center = player.camera.pos;

		glm::mat4 t = glm::inverse(glm::lookAt(eye, center, glm::normalize(player.camera.up)));
		t = glm::translate(t, glm::vec3(-2.5, -1, -.5));
		if (ButtonHeldDown(input->attack))
			t = glm::rotate(t, sin(gameState->time * 20) * 0.5f, { 1,0,-1 });
		t = glm::scale(t, glm::vec3(1, 1, 2));
		t = glm::translate(t, glm::vec3(0, -.5, 0));

		auto entry = MakeRenderEntryTexturedMesh(
			Assets.defaultBox.VAO, Assets.defaultBox.triangleCount, t, polymeshShader, 0, 0, true, glm::vec3(0.1));
		RenderQueuePush(&renderQueue, &entry);
	}

	// draw dropped items
	{
		Texture* texture = GetTexture(TextureAssetID::TestTexture);

		for (size_t i = 0; i < gameState->droppedItems.count; i++)
		{
			Item* item = &gameState->droppedItems[i];
			if (item->count < 1)
				continue;

			glm::mat4 model(1);
			model = glm::translate(model, { 0.0f, sin(gameState->time) * 0.3, 0.0f });
			model = glm::translate(model, item->pos);
			rotateXYZ(model, 0.0f, gameState->time * 30, 0.0f);
			model = glm::scale(model, glm::vec3(0.3));
			model = glm::translate(model, glm::vec3(-0.5)); // center

			RenderEntryMeshShadow entryShadow = MakeRenderEntryMeshShadow(
				Assets.defaultBox.VAO, Assets.defaultBox.triangleCount, 
				model, polymeshShadowShader);

			RenderEntryTexturedMesh entryMain = MakeRenderEntryTexturedMesh(
				Assets.defaultBox.VAO, Assets.defaultBox.triangleCount, 
				model, polymeshShader, 
				texture, &Assets.depthMapFBO.textures[0]);

			RenderQueuePush(&renderQueue, &entryShadow);
			RenderQueuePush(&renderQueue, &entryMain);
		}
	}

#if _DEBUG
	// draw debug geometry
	{
		// axis
		glm::mat4 modelX = glm::scale(glm::mat4(1), {1.0, 0.2, 0.2});
		glm::mat4 modelY = glm::scale(glm::mat4(1), {0.2, 1.0, 0.2});
		glm::mat4 modelZ = glm::scale(glm::mat4(1), {0.2, 0.2, 1.0});


		RenderEntryTexturedMesh entry = MakeRenderEntryTexturedMesh(
			Assets.defaultBox.VAO, Assets.defaultBox.triangleCount, 
			modelX, polymeshShader, 
			0, 0, 
			true, { 1, 0, 0 });
		RenderQueuePush(&renderQueue, &entry);

		entry.color = { 0,1,0 };
		entry.transform = modelY;
		RenderQueuePush(&renderQueue, &entry);

		entry.color = { 0,0,1 };
		entry.transform = modelZ;
		RenderQueuePush(&renderQueue, &entry);
	}
#endif

	//
	// Render Queue
	//

	renderCommandsExecuted = 
		RenderQueueExecute(&renderQueue);

	Renderer::BindFrameBuffer(&Assets.screenFBO);
	Renderer::SetViewportDimensions(fbInfo->sizeX, fbInfo->sizeY);

	// post processing & draw to default buffer
	{
		Renderer::SwitchDepthTest(false);
		Renderer::BindShader(GetShader(ShaderAssetID::ScreenShader));

		glBindFramebuffer(GL_READ_FRAMEBUFFER, Assets.screenFBO.ID);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, Assets.intermediateFBO.ID);
		glBlitFramebuffer(0, 0, fbInfo->sizeX, fbInfo->sizeY, 0, 0, fbInfo->sizeX, fbInfo->sizeY, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		Renderer::BindTexture(&Assets.intermediateFBO.textures[0]);
		Renderer::DrawGeometry(&Assets.screenQuad);
		Renderer::UnbindTexture();

		Renderer::UnbindShader();
		Renderer::SwitchDepthTest(true);
	}

	// draw ui
	UI::Begin(&memory->tempStorage, input, Assets.regularFont, fbInfo);
	Texture* uiAtlas = GetTexture(TextureAssetID::UITexture);
	UI::SetAnchor(uiAnchor::Center, 0);
	{
		UI::DrawElement(uiAtlas, glm::vec3(0, 0, 0), glm::vec3(64, 64, 1), uiCross, {16,16}); // cursor
	}

	int cellsCount = INVENTORY_MAX_SIZE;
	float cellWidth = 70;
	float inventoryWidth = (cellsCount - 1) * cellWidth;
	UI::SetAnchor(uiAnchor::Bottom, 50);
	UI::SetAdvanceMode(AdvanceMode::None);
	UI::ShiftOrigin(-inventoryWidth / 2, 0);
	UI::SetMargin(false);
	for (size_t i = 0; i < player.inventory.cellsCount; i++)
	{
		InventoryCell* cell = &player.inventory.cells[i];

		uiElemType uiElemType = uiStoneBlock;
		uiElemType = GetItemInfo(cell->itemType)->uiElement;

		UI::DrawElement(uiAtlas, glm::vec3(0), glm::vec3(cellWidth, cellWidth, 1), uiElemType, {16, 16}); // TDOO: размеры тайла должны браться из текстуры, а не передаваться

		UI::ShiftOrigin(-FontGetSize(Assets.regularFont) / 2, 0);
		char buf[64];
		sprintf(buf, "%d", cell->itemsCount);
		UI::Text(buf);
		UI::ShiftOrigin(FontGetSize(Assets.regularFont) / 2, 0);
		
		UI::ShiftOrigin(cellWidth, 0);
	}

	UI::SetAdvanceMode(AdvanceMode::Right);
	UI::SetAnchor(uiAnchor::Bottom, 50);
	UI::ShiftOrigin(-inventoryWidth / 2, 0);
	for (size_t i = 0; i < cellsCount; i++)
	{
		UI::DrawElement(uiAtlas, glm::vec3(0), glm::vec3(cellWidth, cellWidth, 1),
			uiInventoryCell, {16, 16}); // inventory
	}

	UI::SetAnchor(uiAnchor::Bottom, 50);
	UI::ShiftOrigin(-inventoryWidth / 2, 0);
	UI::ShiftOrigin(cellWidth* player.inventory.selectedIndex, 0);
	UI::DrawElement(uiAtlas, glm::vec3(0), glm::vec3(cellWidth, cellWidth, 1),
		uiInventorySelectCell, {16,16});

	int heartsCount = 8;
	float heartWidth = 35;
	float heartsWidth = (heartsCount - 1) * heartWidth;
	UI::SetAnchor(uiAnchor::Bottom, 110);
	UI::ShiftOrigin(-heartsWidth / 2, 0);
	for (size_t i = 0; i < heartsCount; i++)
	{
		UI::DrawElement(uiAtlas, glm::vec3(0, 0, 0), glm::vec3(heartWidth, heartWidth, 1), uiHeart, {16,16}); // health bar
	}

	if (gameState->inventoryOpened) {
		UI::SetMargin(true);
		UI::SetAnchor(uiAnchor::Center, 0);
		UI::ShiftOrigin(100, 0);
		UI::SetAdvanceMode(AdvanceMode::Up);
		UI::Text("Craft items");
		for (size_t i = 0; i < ArrayCount(itemInfoTable); i++)
		{

			ItemInfo* itemInfo = &itemInfoTable[i];

			if (!itemInfo->craftable)
				continue;

			Inventory inventoryCopy = player.inventory;
			bool canCraft = true;

			for (size_t j = 0; j < ArrayCount(itemInfo->craftScheme); j++)
			{
				if (itemInfo->craftScheme[j] == ItemType::None)
					continue;

				int cellIndex = InventoryFindItem(&inventoryCopy, itemInfo->craftScheme[j]);
				if (cellIndex != -1) {
					InventoryDropItem(&inventoryCopy, cellIndex, 1);
				}
				else {
					canCraft = false;
					break;
				}
			}

			if (canCraft) {
				char craftButtonText[128];
				sprintf(craftButtonText, "Craft %s", itemInfo->name);
				if (UI::Button(craftButtonText)) {
					player.inventory = inventoryCopy;
					InventoryAddItem(&player.inventory, itemInfo->itemType, 1);
				}
			}
		}
	}

	// debug textures
#if 0
	UI::SetAnchor(uiAnchor::Center, 0);
	UI::ShiftOrigin(-400, 0);
	UI::DrawElement(&Assets.uiAtlas, { 0,0,0 }, { 400,400,400 }, { 1,1 }, { 0,0 });
	UI::DrawElement(&Assets.textureAtlas, { 0,0,0 }, { 400,400,400 }, { 1,1 }, { 0,0 });
	UI::DrawElement(FontGetTextureAtlas(Assets.regularFont), { 0,0,0 }, { 400,400,400 }, { 1,1 }, { 0,0 });
#endif

	// debug shadows
#if 0
	UI::SetAnchor(uiAnchor::Top, 200);
	UI::DrawElement(&Assets.depthMapFBO.textures[0], { 0,0,0, }, { 400, 400, 1 }, { 1,1 }, { 0,0 });
#endif

	// debug info
	if (g_ShowDebugInfo)
	{
		UI::SetAnchor(uiAnchor::LeftTop, 25);
		UI::SetAdvanceMode(AdvanceMode::Down);
		UI::SetMargin(false);
		char buf[128];
		
		sprintf(buf, "FPS: %d", (int)(1.0f / gameState->deltaTime));
		UI::Text(buf);
		sprintf(buf, "Frametime : %.3fms", gameState->deltaTime * 1000);
		UI::Text(buf);

		sprintf(buf, "%d chunks", g_chunkManager.chunksCount);
		UI::Text(buf);
		sprintf(buf, "%d blocks", g_chunkManager.chunksCount * CHUNK_SX * CHUNK_SY * CHUNK_SZ);
		UI::Text(buf);

		sprintf(buf, "%d dropped items", gameState->droppedItems.count);
		UI::Text(buf);

		glm::ivec2 chunkPos = PosToChunkPos(player.camera.pos);
		sprintf(buf, "current chunk x%d z%d", chunkPos.x, chunkPos.y);
		UI::Text(buf);
		
		sprintf(buf, "pos x%.2f y%.2f z%.2f", player.camera.pos.x, player.camera.pos.y, player.camera.pos.z);
		UI::Text(buf);
		sprintf(buf, "orient x%.2f y%.2f z%.2f", player.camera.front.x, player.camera.front.y, player.camera.front.z);
		UI::Text(buf);
		sprintf(buf, "light dir x%.2f y%.2f z%.2f", lighting.directLightDirection.x, lighting.directLightDirection.y, lighting.directLightDirection.z);
		UI::Text(buf);

		sprintf(buf, "memory perm %.1f/%.1fMB, temp %.1f/%.1fMB, chunks %.1f/%.1fMB",
			(float)memory->permStorage.size / Megabytes(1),
			(float)memory->permStorage.capacity / Megabytes(1),
			(float)memory->tempStorage.size / Megabytes(1),
			(float)memory->tempStorage.capacity / Megabytes(1),
			(float)memory->chunkStorage.size / Megabytes(1),
			(float)memory->chunkStorage.capacity / Megabytes(1)
		);
		UI::Text(buf);

		sprintf(buf, "%d render queue commands", renderCommandsExecuted);
		UI::Text(buf);
		sprintf(buf, "%d draw calls, %d triangles rendered", g_RendererStats.drawCallsCount + g_RendererStats.drawCallsInstancedCount, g_RendererStats.trianglesRendered);
		UI::Text(buf);
	}

	UI::End();

#pragma endregion

}