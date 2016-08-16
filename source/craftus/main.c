#include "craftus/entity/player.h"
#include "craftus/render/render.h"
#include "craftus/world/chunkworker.h"
#include "craftus/world/world.h"
#include "craftus/world/worldgen.h"

#include <3ds.h>
#include <citro3d.h>

#include <stdio.h>

static bool generateFlatWorld_test(ChunkWorker_Queue* queue, ChunkWorker_Task task) {
	Chunk* c = task.chunk;
	for (int y = 0; y < CHUNK_HEIGHT; y++)
		for (int z = 0; z < CHUNK_DEPTH; z++)
			for (int x = 0; x < CHUNK_WIDTH; x++) {
				Block b = Block_Air;
				switch (y) {
					case 0 ... 8:
						b = Block_Stone;
						break;
					case 9 ... 14:
						b = Block_Dirt;
						break;
					case 15:
						b = Block_Grass;
						break;
					default:
						return false;
				}
				Chunk_SetBlock(c, x, y, z, b);
			}
	return false;
}

ChunkWorker* cworker;
int main(int argc, char* argv[]) {
	HIDUSER_EnableGyroscope();

	gfxInitDefault();

	PrintConsole consoleStatus;
	PrintConsole consoleEvent;

	consoleInit(GFX_BOTTOM, &consoleStatus);
	consoleInit(GFX_BOTTOM, &consoleEvent);

	consoleSetWindow(&consoleStatus, 0, 0, 320 / 8, 240 / 16);
	consoleSetWindow(&consoleEvent, 0, 240 / 16 + 1, 320 / 8, 240 / 16 - 1);

	consoleSelect(&consoleEvent);

	printf("Craftus3D running!");

	osSetSpeedupEnable(true);
	if (R_FAILED(APT_SetAppCpuTimeLimit(30))) {
		printf("Failed to set AppCpuTimeLimit\n");
	}

	if (R_FAILED(romfsInit())) {
		printf("RomFS init failed!\n");
	}

	Render_Init();

	World* world = World_New();
	WorldGen_Setup(world);

	cworker = ChunkWorker_New(world);

	if (world->genConfig.type == World_GenNormal) {
		ChunkWorker_AddHandler(cworker, ChunkWorker_TaskDecorateChunk, &WorldGen_ChunkBaseGenerator, 0);
	} else {
		ChunkWorker_AddHandler(cworker, ChunkWorker_TaskDecorateChunk, &generateFlatWorld_test, 0);
	}

	Player* player = Player_New();
	Player_Spawn(player, world);

	consoleSelect(&consoleStatus);
	float max = cworker->queue[cworker->currentQueue].length + cworker->queue[cworker->currentQueue ^ 1].length;
	// Hier könnte später ein Ladebildschirm hin("Welt wird geladen, Landschaft wird generiert")
	while (cworker->queue[cworker->currentQueue].length > 0 || cworker->queue[cworker->currentQueue ^ 1].length > 0) {
		float current = cworker->queue[cworker->currentQueue].length + cworker->queue[cworker->currentQueue ^ 1].length;
		consoleClear();
		printf("Generating world %d%%\n", 100 - (int)((current / max) * 100.f));
		svcSleepThread(800000);
	}

	for (int x = 0; x < CACHE_SIZE; x++) {
		for (int z = 0; z < CACHE_SIZE; z++) {
			if (world->cache[0]->cache[x][z]->flags & ClusterFlags_VBODirty)
				if (BlockRender_PolygonizeChunk(world, world->cache[0]->cache[x][z])) {
					world->cache[0]->cache[x][z]->flags &= ~ClusterFlags_VBODirty;
				}
		}
		printf("Row %d finished\n", x);
	}

	u64 time = osGetTime(), tickClock = 0, deltaTime = 0, fpsClock = 0;
	u32 fps = 0, fpsCounter = 0;

	while (aptMainLoop()) {
		consoleSelect(&consoleEvent);
		if (tickClock >= 50) {
			World_Tick(world);

			tickClock = 0;
		}
		if (fpsClock >= 1000) {
			fps = fpsCounter;
			fpsClock = 0, fpsCounter = 0;
		}

		hidScanInput();

		// gfxFlushBuffers();

		consoleSelect(&consoleStatus);
		consoleClear();

		printf("Player: %f, %f, %f \n", player->x, player->y, player->z);
		printf("FPS: %d", fps);

		u32 input = hidKeysHeld();
		if (input & KEY_START) break;

		World_Profile(world);

		Player_Update(player, input, (float)deltaTime / 1000.f);

		// gfxFlushBuffers();

		// consoleSelect(&consoleEvent);

		Render(player);

		u64 timeNow = osGetTime();
		deltaTime = timeNow - time;

		tickClock += deltaTime;
		fpsClock += deltaTime;

		fpsCounter++;

		time = timeNow;
	}

	ChunkWorker_Stop(cworker);

	Player_Free(player);
	World_Free(world);

	Render_Exit();

	romfsExit();

	gfxExit();

	HIDUSER_DisableAccelerometer();

	return 0;
}
