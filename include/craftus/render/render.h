#ifndef RENDER_H_INCLUDED
#define RENDER_H_INCLUDED

#include <stdbool.h>

#include "craftus/world/world.h"
#include "craftus/entity/player.h"

void Render_Init();
void Render_Exit();

void Render(Player* player);

bool BlockRender_PolygonizeChunk(World* world, Chunk* chunk);

typedef struct {
	int16_t xz[2];
	uint8_t yuvb[4];
} world_vertex;

#endif  // !RENDER_H_INCLUDED