#include "craftus/world/worldgen.h"

#include <stdio.h>

static WorldGen_GeneratorSetup setup;
void WorldGen_Setup(World* world) {
	setup.world = world;
	snoise_setup_perm(&setup.permTable, world->genConfig.seed);
}

static inline float lerp(float start, float end, float t) { return start + ((end - start) * t); }
static inline float bilerp(float q11, float q21, float q12, float q22, float x, float y) { return lerp(lerp(q11, q21, x), lerp(q12, q22, x), y); }
static inline float trilerp(float q111, float q211, float q121, float q221, float q112, float q212, float q122, float q222, float x, float y, float z) {
	lerp(bilerp(q111, q211, q112, q212, x, z), bilerp(q121, q221, q122, q222, x, z), y);
}

float octave_noise(int octaves, float persistence, float scale, float x, float y) {
	float total = 0;
	float frequency = scale;
	float amplitude = 1;

	// We have to keep track of the largest possible amplitude,
	// because each octave adds more, and we need a value in [-1, 1].
	float maxAmplitude = 0;

	for (int i = 0; i < octaves; i++) {
		total += snoise2(&setup.permTable, x * frequency, y * frequency) * amplitude;

		frequency *= 2;
		maxAmplitude += amplitude;
		amplitude *= persistence;
	}

	return total / maxAmplitude;
}

bool WorldGen_ChunkBaseGenerator(ChunkWorker_Queue* queue, ChunkWorker_Task task) {
	float offsetX = task.chunk->x * CHUNK_WIDTH, offsetZ = task.chunk->z * CHUNK_DEPTH;

	for (int x = 0; x < CHUNK_WIDTH; x++) {
		for (int z = 0; z < CHUNK_DEPTH; z++) {
			int lutAdr = x / 4;
			float subrangeX = (float)(x % 4) * 0.25f, subrangeZ = (float)(z % 4) * 0.25f;
			float v = snoise2(&setup.permTable, (offsetX + x) * 0.05f, (offsetZ + z) * 0.05f) * 16.f;
			for (int y = 0; y < (int)v + WORLDGEN_SEALEVEL; y++) {
				if (y > WORLDGEN_SEALEVEL - 24) {
					float v2 = snoise3(&setup.permTable, (offsetX + x + 10) * 0.025f, (float)y * 0.1f, (offsetZ + z + 10) * 0.025f);
					if (v2 * 10.f > 0.2f) {
						Chunk_SetBlock(task.chunk, x, y, z, Block_Stone);
					}
				} else
					Chunk_SetBlock(task.chunk, x, y, z, Block_Stone);
			}

			// Mit Grass und Erde füllen
			Block previousBlock = Block_Air;
			for (int y = (int)v + WORLDGEN_SEALEVEL + 1; y > WORLDGEN_SEALEVEL - 10; y--) {
				Block block = Chunk_GetBlock(task.chunk, x, y, z);
				if (previousBlock == Block_Air && block == Block_Stone) {
					Chunk_SetBlock(task.chunk, x, y, z, Block_Grass);
					y--;
					for (int i = 0; i < 5; i++) {
						block = Chunk_GetBlock(task.chunk, x, y - i, z);
						if (block == Block_Air) break;
						Chunk_SetBlock(task.chunk, x, y - i, z, Block_Dirt);
						block = Block_Dirt;
					}
				}
				previousBlock = block;
			}
		}
	}

	return false;
}