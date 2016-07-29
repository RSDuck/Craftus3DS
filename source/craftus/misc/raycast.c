#include "craftus/misc/raycast.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define INF 100.f

bool Raycast_Cast(World* world, C3D_FVec inpos, C3D_FVec raydir, Raycast_Result* out) {
	int mapX = FastFloor(inpos.x), mapY = FastFloor(inpos.y), mapZ = FastFloor(inpos.z);

	float xSqr = raydir.x * raydir.x;
	float ySqr = raydir.y * raydir.y;
	float zSqr = raydir.z * raydir.z;

	float deltaDistX = sqrtf(1.f + (ySqr + zSqr) / xSqr);
	float deltaDistY = sqrtf(1.f + (xSqr + zSqr) / ySqr);
	float deltaDistZ = sqrtf(1.f + (xSqr + ySqr) / zSqr);

	int stepX, stepY, stepZ;
	float sideDistX, sideDistY, sideDistZ;
	if (raydir.x < 0) {
		stepX = -1;
		sideDistX = (inpos.x - mapX) * deltaDistX;
	} else {
		stepX = 1;
		sideDistX = (mapX + 1.f - inpos.x) * deltaDistX;
	}
	if (raydir.y < 0) {
		stepY = -1;
		sideDistY = (inpos.y - mapY) * deltaDistY;
	} else {
		stepY = 1;
		sideDistY = (mapY + 1.f - inpos.y) * deltaDistY;
	}
	if (raydir.z < 0) {
		stepZ = -1;
		sideDistZ = (inpos.z - mapZ) * deltaDistZ;
	} else {
		stepZ = 1;
		sideDistZ = (mapZ + 1.f - inpos.z) * deltaDistZ;
	}

	int hit = 0, side;
	while (hit == 0) {
		if (sideDistX < sideDistY && sideDistX < sideDistZ) {
			sideDistX += deltaDistX;
			mapX += stepX;
			side = 0;
		} else if (sideDistY < sideDistZ) {
			sideDistY += deltaDistY;
			mapY += stepY;
			side = 1;
		} else {
			sideDistZ += deltaDistZ;
			mapZ += stepZ;
			side = 2;
		}
		if (World_GetBlock(world, mapX, mapY, mapZ) != Block_Air) hit = 1;

		if (world->errFlags & World_ErrUnloadedBlockRequested) break;
	}

	out->x = mapX;
	out->y = mapY;
	out->z = mapZ;
	out->direction = 0;

	return hit;
}