#include "craftus/entity/player.h"

#include <string.h>
#include <math.h>
#include <stdio.h>

#include <3ds.h>

#include "craftus/misc/raycast.h"

Player* Player_New() {
	Player* player = malloc(sizeof(Player));
	player->x = 0.f;
	player->y = 65.f;
	player->z = 0.f;
	player->yaw = 0.f;
	player->pitch = 0.f;
	player->bobbing = 0.f;
	player->cache = malloc(sizeof(ChunkCache));
	player->world = NULL;

	return player;
}

void Player_Free(Player* player) {
	free(player->cache);
	free(player);
}

void Player_Spawn(Player* player, World* world) {
	player->world = world;
	world->cache[0] = player->cache;
	Player_Moved(player);
	player->cache->where = player->cache->updatedWhere;

	for (int x = -(CACHE_SIZE / 2); x <= CACHE_SIZE / 2; x++) {
		for (int y = -(CACHE_SIZE / 2); y <= CACHE_SIZE / 2; y++) {
			world->cache[0]->cache[x + (CACHE_SIZE / 2)][y + (CACHE_SIZE / 2)] = World_GetChunk(world, x, y);
		}
	}
}

static u32 lastKeys;

const float DEG_TO_RAD = M_PI * (1.f / 180.f);

static float blockBreakBuildTimeout = 0.f;
void Player_Update(Player* player, u32 input, float deltaTime) {
	float dx = 0.f, dy = -10.f, dz = 0.f;

	circlePosition circ;
	hidCircleRead(&circ);

	float circX = fabsf((float)circ.dx / 156.f);
	float circY = fabsf((float)circ.dy / 156.f);

	if (input & KEY_R) {
		// Strafing
		if (input & KEY_CPAD_RIGHT) {
			dx += sinf(player->yaw + (M_PI / 2.f)) * circX * 4.f;
			dz += cosf(player->yaw + (M_PI / 2.f)) * circX * 4.f;
		}
		if (input & KEY_CPAD_LEFT) {
			dx += sinf(player->yaw - (M_PI / 2.f)) * circX * 4.f;
			dz += cosf(player->yaw - (M_PI / 2.f)) * circX * 4.f;
		}
	} else {
		// Drehen nach links und rechts
		if (input & KEY_CPAD_LEFT) player->yaw += 100.f * DEG_TO_RAD * circX * deltaTime;
		if (input & KEY_CPAD_RIGHT) player->yaw -= 100.f * DEG_TO_RAD * circX * deltaTime;
	}

	// Vorwärts und Rückwärts
	if (input & KEY_CPAD_UP) {
		dx -= sinf(player->yaw) * circY * 4.f;
		dz -= cosf(player->yaw) * circY * 4.f;
	}
	if (input & KEY_CPAD_DOWN) {
		dx += sinf(player->yaw) * circY * 4.f;
		dz += cosf(player->yaw) * circY * 4.f;
	}

	// Drehen nach oben und unten
	if (input & KEY_X) player->pitch += 100.f * DEG_TO_RAD * deltaTime;
	if (input & KEY_B) player->pitch -= 100.f * DEG_TO_RAD * deltaTime;

	if (input & KEY_L) dy += 16.f;

	dy *= deltaTime, dx *= deltaTime, dz *= deltaTime;

#define SIGN(x, y) ((x) < 0 ? (y) * -1 : (y))
	if (dy < 0.f && World_GetBlock(player->world, FastFloor(player->x), FastFloor(player->y + dy), FastFloor(player->z)) == Block_Air) player->y += dy;
	if (dy > 0.f && World_GetBlock(player->world, FastFloor(player->x), FastFloor(player->y + dy + PLAYER_HEIGHT), FastFloor(player->z)) == Block_Air) player->y += dy;

	if (dx != 0.f || dz != 0.f) {
		if (World_GetBlock(player->world, FastFloor(player->x + dx + SIGN(dx, 0.1)), FastFloor(player->y), FastFloor(player->z)) == Block_Air &&
		    World_GetBlock(player->world, FastFloor(player->x + dx + SIGN(dx, 0.1)), FastFloor(player->y) + 1, FastFloor(player->z)) == Block_Air)
			player->x += dx;
		if (World_GetBlock(player->world, FastFloor(player->x), FastFloor(player->y), FastFloor(player->z + dz + SIGN(dz, 0.1))) == Block_Air &&
		    World_GetBlock(player->world, FastFloor(player->x), FastFloor(player->y) + 1, FastFloor(player->z + dz + SIGN(dz, 0.1))) == Block_Air)
			player->z += dz;

		player->bobbing += (360.f * 6.f) * DEG_TO_RAD * deltaTime;
		if (player->bobbing >= 360.f * DEG_TO_RAD) player->bobbing = 0.f;  // Anti Overflow

		Player_Moved(player);
	}

	Raycast_Result rayRes;
	C3D_FVec final = {0.f, 0.f, 0.f, 0.f};
	final.x = -sinf(player->yaw) * cosf(player->pitch);
	final.y = sinf(player->pitch);
	final.z = -cosf(player->yaw) * cosf(player->pitch);

	bool hit = Raycast_Cast(player->world, (C3D_FVec){1.f, player->z, player->y + PLAYER_EYE_HEIGHT, player->x}, final, &rayRes);
	if (hit) printf("Ray hit a %d, %d, %d %f %d\n", rayRes.x, rayRes.y, rayRes.z, rayRes.distSqr, rayRes.direction);

	if (blockBreakBuildTimeout <= 0.f && hit && rayRes.distSqr <= (5.f * 5.f + 5.f * 5.f + 5.f * 5.f)) {
		if (input & KEY_Y) {
			World_SetBlock(player->world, rayRes.x, rayRes.y, rayRes.z, Block_Air);
		}
		if (input & KEY_A) {
			const int* blockOffset = DirectionToPosition[rayRes.direction];
			World_SetBlock(player->world, rayRes.x + blockOffset[0], rayRes.y + blockOffset[1], rayRes.z + blockOffset[2], Block_Stone);
		}
		blockBreakBuildTimeout = 0.15f;
	}
	blockBreakBuildTimeout -= deltaTime;

	lastKeys = input;
}

void Player_Moved(Player* player) {
	ChunkCache* cache = player->cache;

	cache->updatedWhere.startX = ((int)player->x / CHUNK_WIDTH) - (CACHE_SIZE / 2);
	cache->updatedWhere.startY = ((int)player->z / CHUNK_DEPTH) - (CACHE_SIZE / 2);

	cache->updatedWhere.endX = player->cache->updatedWhere.startX + CACHE_SIZE;
	cache->updatedWhere.endY = player->cache->updatedWhere.startY + CACHE_SIZE;
}
