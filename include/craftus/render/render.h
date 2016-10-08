#ifndef RENDER_H_INCLUDED
#define RENDER_H_INCLUDED

#include <stdbool.h>

#include "craftus/entity/player.h"
#include "craftus/world/world.h"

#include "craftus/world/chunkworker.h"

void Render_Init();
void Render_Exit();

void Render(Player* player);

bool BlockRender_PolygonizeChunk(World* world, Chunk* chunk, bool progressive);

void BlockRender_Init();
void BlockRender_Free();

bool BlockRender_TaskPolygonizeChunk(ChunkWorker_Queue* queue, ChunkWorker_Task task);
void BlockRender_PutBuffersIntoPlace(World* world);

typedef struct {
	int16_t xz[2];
	uint8_t yuvb[4];
} world_vertex;

#endif  // !RENDER_H_INCLUDED