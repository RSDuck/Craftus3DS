#ifndef RENDER_H_INCLUDED
#define RENDER_H_INCLUDED

#include <stdbool.h>

#include "craftus/world/world.h"
#include "craftus/entity/player.h"

void Render_Init();
void Render_Exit();

void Render(Player* player);

bool BlockRender_PolygonizeChunk(World* world, Chunk* chunk);

void BlockRender_Init();
void BlockRender_Free();

typedef struct {
	float offset[2];  // XZ Offset
	uint8_t pointA[3];
	uint8_t pointB[3];
	uint8_t uvOffset[2];
	uint8_t brightness[4];
} world_vertex;

#endif  // !RENDER_H_INCLUDED