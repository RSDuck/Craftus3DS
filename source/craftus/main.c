#include "craftus/world/world.h"
#include "craftus/entity/player.h"
#include "craftus/render/render.h"
#include "craftus/world/chunkworker.h"

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
					case 0 ... 1:
						b = Block_Stone;
						break;
					case 2 ... 6:
						b = Block_Dirt;
						break;
					case 7:
						b = Block_Grass;
						break;
					default:
						return false;
				}
				Chunk_SetBlock(c, x, y, z, b);
			}
}

ChunkWorker* cworker;
int main(int argc, char* argv[]) {
	HIDUSER_EnableGyroscope();

	gfxInitDefault();

	PrintConsole consoleStatus;
	PrintConsole consoleEvent;

	consoleInit(GFX_BOTTOM, &consoleStatus);
	consoleInit(GFX_BOTTOM, &consoleEvent);

	printf("Craftus3D running!");

	osSetSpeedupEnable(true);
	if (R_FAILED(APT_SetAppCpuTimeLimit(30))) {
		printf("Failed to set AppCpuTimeLimit\n");
	}

	if (R_FAILED(romfsInit())) {
		printf("RomFS init failed!");
	}

	Render_Init();
	consoleSetWindow(&consoleStatus, 0, 0, 320 / 8, 240 / 16);
	consoleSetWindow(&consoleEvent, 0, 240 / 16 + 1, 320 / 8, 240 / 16 - 1);

	World* world = World_New();

	cworker = ChunkWorker_New(world);
	ChunkWorker_AddHandler(cworker, ChunkWorker_TaskDecorateChunk, &generateFlatWorld_test, 0);

	Player* player = Player_New();
	Player_Spawn(player, world);

	// Hier könnte später ein Ladebildschirm hin("Welt wird geladen, Landschaft wird generiert")
	while (cworker->queue[cworker->currentQueue].length > 0 || cworker->queue[cworker->currentQueue ^ 1].length > 0) {
		svcSleepThread(200000);
	}

	u64 time = osGetTime(), tickClock = 0, deltaTime = 0, fpsClock = 0;
	u32 fps = 0, fpsCounter = 0;

	for (float i = 0; i < M_PI * 2; i += 1.f * M_PI / 180.f) {
		World_SetBlock(player->world, FastFloor(sinf(i) * 16.f), 8, FastFloor(cosf(i) * 16.f), Block_Dirt);
	}

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

		gfxFlushBuffers();

		consoleSelect(&consoleStatus);
		consoleClear();

		printf("Player: %f, %f, %f \n", player->x, player->y, player->z);
		printf("FPS: %d", fps);

		u32 input = hidKeysHeld();
		if (input & KEY_START) break;

		World_Profile(world);

		Player_Update(player, input, (float)deltaTime / 1000.f);

		gfxFlushBuffers();

		consoleSelect(&consoleEvent);

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