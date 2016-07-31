#include "craftus/render/camera.h"

#include "craftus/render/vecmath.h"

void Camera_Init(Camera* cam) {
	Mtx_Identity(&cam->view);

	cam->fov = 70.f * M_PI / 180.f;
	cam->near = 0.01f, cam->far = 1000.f;

	Mtx_PerspTilt(&cam->projection, cam->fov, 400.f / 240.f, cam->near, cam->far);
}

void Camera_Update(Camera* cam, Player* player, float iod) {
	Mtx_PerspStereoTilt(&cam->projection, cam->fov, 400 / 240.f, cam->near, cam->far, iod, 2.f);

	Mtx_Identity(&cam->view);
	Mtx_RotateX(&cam->view, -player->pitch, true);
	Mtx_RotateY(&cam->view, -player->yaw, true);
	Mtx_Translate(&cam->view, -player->x, -(player->y + PLAYER_EYE_HEIGHT + sinf(player->bobbing) * 0.05f), -player->z);

	C3D_Mtx vp;
	Mtx_Multiply(&vp, &cam->projection, &cam->view);

	C3D_FVec rowX = vp.r[0];
	C3D_FVec rowY = vp.r[1];
	C3D_FVec rowZ = vp.r[2];
	C3D_FVec rowW = vp.r[3];

	cam->frustumPlanes[Frustum_Near] = FVec_Sub(rowW, rowZ);
	cam->frustumPlanes[Frustum_Right] = FVec_Add(rowW, rowX);
	cam->frustumPlanes[Frustum_Left] = FVec_Sub(rowW, rowX);
	cam->frustumPlanes[Frustum_Top] = FVec_Add(rowW, rowY);
	cam->frustumPlanes[Frustum_Bottom] = FVec_Sub(rowW, rowY);
	cam->frustumPlanes[Frustum_Far] = FVec_Add(rowW, rowZ);

	for (int i = 0; i < Frustum_Count; i++) {
		float length = FVec_Mod3(&cam->frustumPlanes[i]);
		cam->frustumPlanes[i].x /= length;
		cam->frustumPlanes[i].y /= length;
		cam->frustumPlanes[i].z /= length;
		cam->frustumPlanes[i].w /= length;
	}
}

bool Camera_IsPointVisible(Camera* cam, C3D_FVec point) {
	point.w = 1.f;
	for (int i = 0; i < Frustum_Count; i++)
		if (FVec_DP4(&point, &cam->frustumPlanes[i]) < 0.f) return false;
	return true;
}

#include <stdio.h>

bool Camera_IsAABBVisible(Camera* cam, C3D_FVec orgin, C3D_FVec size) {
	/*int in, out;
	for (int i = 0; i < 6; i++) {
		out = 0, in = 0;
		for (int j = 0; j < 8 && (!in || !out); j++) {
			C3D_FVec point = FVec_Add(orgin, FVec_Mul(box[j], size));
			point.w = 1.f;
			if (FVec_DP3(&point, &cam->frustumPlanes[i]) <= 0.f)
				out++;
			else
				in++;

			if (!in) return false;
		}
	}
	return true;*/
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