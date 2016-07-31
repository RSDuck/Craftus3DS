#include "craftus/world/worldgen.h"

#include <stdio.h>

static WorldGen_GeneratorSetup setup;
void WorldGen_Setup(World* world) {
	setup.world = world;
	snoise_setup_perm(&setup.permTable, world->genConfig.seed);
}

static inline float lerp(float start, float end, float t) { return start + ((end - start) * t); }
static inline float bilerp(float q11, float q21, float q12, float q22, float x, float y) { return lerp(lerp(q11, q21, x), lerp(q12, q22, x), y); }

bool WorldGen_ChunkBaseGenerator(ChunkWorker_Queue* queue, ChunkWorker_Task task) {
	float topPoints[5][5];

	float chunkX = task.chunk->x * CHUNK_WIDTH, chunkZ = task.chunk->z * CHUNK_DEPTH;
	for (int x = 0; x < CHUNK_WIDTH + 1; x += CHUNK_WIDTH / 4)
		for (int z = 0; z < CHUNK_DEPTH + 1; z += CHUNK_DEPTH / 4) {
			topPoints[x / 4][z / 4] = snoise2(&setup.permTable, (float)x + chunkX, (float)z + chunkZ) * 20.f;
		}

	for (int x = 0; x < CHUNK_WIDTH; x++) {
		float px = (float)(x % 4) * 0.25f;
		for (int z = 0; z < CHUNK_DEPTH; z++) {
			float pz = (float)(z % 4) * 0.25f;
			int height = (int)bilerp(topPoints[x / 4][z / 4], topPoints[x / 4 + 1][z / 4], topPoints[x / 4][z / 4 + 1], topPoints[x / 4 + 1][z / 4 + 1], px, pz);
			height += WORLDGEN_SEALEVEL;
			// printf("%d\n", height);
			for (int y = 0; y < height; y++) {
				Chunk_SetBlock(task.chunk, x, y, z, Block_Stone);
			}
		}
	}
	return false;
}