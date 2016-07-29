#ifndef CAMERA_H_INCLUDED
#define CAMERA_H_INCLUDED

#include <stdbool.h>

#include <citro3d.h>

#include "craftus/entity/player.h"

enum FrustumPlanes {
	Frustum_Near = 0,
	Frustum_Right,
	Frustum_Left,
	Frustum_Top,
	Frustum_Bottom,
	Frustum_Far,

	Frustum_Count
};

typedef struct {
	C3D_Mtx projection, view;
	C3D_FVec frustumPlanes[Frustum_Count];

	float near, far, fov;
} Camera;

void Camera_Init(Camera* cam);
void Camera_Update(Camera* cam, Player* player, float iod);

bool Camera_IsPointVisible(Camera* cam, C3D_FVec point);
bool Camera_IsAABBVisible(Camera* cam, C3D_FVec orgin, C3D_FVec size);

#endif  // !CAMERA_H_INCLUDED