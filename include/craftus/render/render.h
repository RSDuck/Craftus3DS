#ifndef RENDER_H_INCLUDED
#define RENDER_H_INCLUDED

#include "craftus/world/world.h"
#include "craftus/entity/player.h"

void Render_Init();
void Render_Exit();

void Render(Player* player);

bool BlockRender_PolygonizeChunk(World* world, Chunk* chunk);

typedef struct {
	float position[3];
	float uv[2];
	float color[3];
} world_vertex;


#endif  // !RENDER_H_INCLUDED