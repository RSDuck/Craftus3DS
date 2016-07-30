#include "craftus/world/worldgen.h"

#include <stdio.h>

static WorldGen_GeneratorSetup setup;
void WorldGen_Setup(World* world) {
	setup.world = world;
	snoise_setup_perm(&setup.permTable, world->genConfig.seed);
}

static inline float trilerp(float start, float end, float t) { return start + (end - start) * t; }

bool WorldGen_ChunkBaseGenerator(ChunkWorker_Queue* queue, ChunkWorker_Task task) {
	for (int x = 0; x < CHUNK_WIDTH; x++) {
		for (int z = 0; z < CHUNK_DEPTH; z++) {
			for (int y = 0; y < (CHUNK_HEIGHT / 3) * 2; y++) {
				Chunk_SetBlock(task.chunk, x, y, z, Block_Stone);
			}
		}
	}
	return false;
}