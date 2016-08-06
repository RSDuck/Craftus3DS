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
	float offset[2];	// XZ Offset
	uint8_t du[4];		// Eine Seite, [4] = Y Wert
	uint8_t dv[4];		// Andere Seite, [4] = Backface
	uint8_t uvOrgin[2];     // U * 255, V * 255
	uint8_t brightness[4];  // Beleuchtung jeder Seite
} world_vertex;

inline world_vertex world_vertex_create(float x, float y, float z, bool backface, int du[], int dv[], uint8_t u, uint8_t v, uint8_t brightness[]) {
	return (world_vertex){{x, z},
			      {(uint8_t)du[0], (uint8_t)du[1], (uint8_t)du[2], (uint8_t)y},
			      {(uint8_t)dv[0], (uint8_t)dv[1], (uint8_t)dv[2], backface},
			      {u, v},
			      {brightness[0], brightness[1], brightness[2], brightness[3]}};
}

#endif  // !RENDER_H_INCLUDED