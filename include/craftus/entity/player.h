#ifndef PLAYER_H_INCLUDED
#define PLAYER_H_INCLUDED

#include "craftus/world/world.h"

#include <3ds/types.h>

#define PLAYER_HEIGHT 1.8f
#define PLAYER_EYE_HEIGHT 1.72f

#define PLAYER_SEIGHT_INF -42

typedef struct {
	float x, y, z;

	int seightX, seightY, seightZ;
	// float seightDistance;
	Direction seightDirection;

	float velocity;

	float velX, velZ;

	float jumpTimer;

	float flyingControl;

	float viewVecX, viewVecY, viewVecZ;
	float pitch, yaw;

	float bobbing;

	World* world;
	ChunkCache* cache;

	bool flying, grounded;

	Block selectedBlock;
} Player;

Player* Player_New();
void Player_Free(Player* player);

void Player_Spawn(Player* player, World* world);
void Player_Moved(Player* player);
void Player_Update(Player* player, u32 input, float deltaTime);

#endif  // !PLAYER_H_INCLUDED