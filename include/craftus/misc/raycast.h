#ifndef RAYCAST_H_INCLUDED

#include <math.h>
#include <stdbool.h>

#include <citro3d.h>

#include "craftus/world/world.h"

typedef struct {
	int x, y, z;
	Direction direction;
	float distSqr;
} Raycast_Result;

bool Raycast_Cast(World* world, C3D_FVec inpos, C3D_FVec raydir, Raycast_Result* out);

#endif  // !RAYCAST_H_INCLUDED