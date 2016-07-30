#ifndef WORLDGEN_H_INCLUDED

#include <SimplexNoise/simplexnoise.h>

#include "craftus/world/world.h"
#include "craftus/world/chunkworker.h"

typedef struct {
	World* world;
	snoise_permtable permTable;
} WorldGen_GeneratorSetup;

void WorldGen_Setup(World* world);
bool WorldGen_ChunkBaseGenerator(ChunkWorker_Queue* queue, ChunkWorker_Task task);

#endif  // !WORLDGEN_H_INCLUDED