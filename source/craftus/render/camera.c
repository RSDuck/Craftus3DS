#include "craftus/render/camera.h"

void Camera_Init(Camera* cam) {
	Mtx_Identity(&cam->view);

	cam->fov = C3D_AngleFromDegrees(60.f);
	cam->near = 0.01f, cam->far = 3.f * CHUNK_WIDTH - 4.f;

	Mtx_PerspTilt(&cam->projection, cam->fov, ((400.f / 2.f) / (240.f / 2.f)), cam->near, cam->far, false);
}

void Camera_Update(Camera* cam, Player* player, float iod) {
	Mtx_PerspStereoTilt(&cam->projection, cam->fov, ((400.f / 2.f) / (240.f / 2.f)), cam->near, cam->far, iod, 2.f, false);

	Mtx_Identity(&cam->view);
	Mtx_RotateX(&cam->view, -player->pitch, true);
	Mtx_RotateY(&cam->view, -player->yaw, true);
	Mtx_Translate(&cam->view, -player->x, -(player->y + PLAYER_EYE_HEIGHT + sinf(player->bobbing) * 0.05f), -player->z, true);

	C3D_Mtx vp;
	Mtx_Multiply(&vp, &cam->projection, &cam->view);
	// Mtx_Copy(&vp, &cam->projection);

	C3D_FVec rowX = vp.r[0];
	C3D_FVec rowY = vp.r[1];
	C3D_FVec rowZ = vp.r[2];
	C3D_FVec rowW = vp.r[3];

	cam->frustumPlanes[Frustum_Near] = FVec4_Normalize(FVec4_Subtract(rowW, rowZ));
	cam->frustumPlanes[Frustum_Right] = FVec4_Normalize(FVec4_Add(rowW, rowX));
	cam->frustumPlanes[Frustum_Left] = FVec4_Normalize(FVec4_Subtract(rowW, rowX));
	cam->frustumPlanes[Frustum_Top] = FVec4_Normalize(FVec4_Add(rowW, rowY));
	cam->frustumPlanes[Frustum_Bottom] = FVec4_Normalize(FVec4_Subtract(rowW, rowY));
	cam->frustumPlanes[Frustum_Far] = FVec4_Normalize(FVec4_Add(rowW, rowZ));
}

bool Camera_IsPointVisible(Camera* cam, C3D_FVec point) {
	point.w = 1.f;
	for (int i = 0; i < Frustum_Count; i++)
		if (FVec4_Dot(point, cam->frustumPlanes[i]) < 0.f) return false;
	return true;
}

bool Camera_IsAABBVisible(Camera* cam, C3D_FVec orgin, C3D_FVec size) {
	float x = orgin.x, y = orgin.y, z = orgin.z;
	C3D_FVec* frustum = cam->frustumPlanes;
	int p;

	for (p = 0; p < 6; p++) {
		if (frustum[p].x * (x - size.x) + frustum[p].y * (y - size.y) + frustum[p].z * (z - size.z) + frustum[p].w > 0) continue;
		if (frustum[p].x * (x + size.x) + frustum[p].y * (y - size.y) + frustum[p].z * (z - size.z) + frustum[p].w > 0) continue;
		if (frustum[p].x * (x - size.x) + frustum[p].y * (y + size.y) + frustum[p].z * (z - size.z) + frustum[p].w > 0) continue;
		if (frustum[p].x * (x + size.x) + frustum[p].y * (y + size.y) + frustum[p].z * (z - size.z) + frustum[p].w > 0) continue;
		if (frustum[p].x * (x - size.x) + frustum[p].y * (y - size.y) + frustum[p].z * (z + size.z) + frustum[p].w > 0) continue;
		if (frustum[p].x * (x + size.x) + frustum[p].y * (y - size.y) + frustum[p].z * (z + size.z) + frustum[p].w > 0) continue;
		if (frustum[p].x * (x - size.x) + frustum[p].y * (y + size.y) + frustum[p].z * (z + size.z) + frustum[p].w > 0) continue;
		if (frustum[p].x * (x + size.x) + frustum[p].y * (y + size.y) + frustum[p].z * (z + size.z) + frustum[p].w > 0) continue;
		return false;
	}

	return true;
}