#include "craftus/render/render.h"

#include "craftus/render/vecmath.h"

#include <citro3d.h>

#include <stdio.h>

#include "vec/vec.h"

typedef struct {
	u8 x, y, z, sideAO, size;
	Block block;
} Side;

static inline Block fastBlockFetch(World* world, Chunk* chunk, Cluster* cluster, int x, int y, int z) {
	return (x < 0 || y < 0 || z < 0 || x >= CHUNK_WIDTH || y >= CHUNK_CLUSTER_HEIGHT || z >= CHUNK_DEPTH)
		   ? World_GetBlock(world, (chunk->x * CHUNK_WIDTH) + x, (cluster->y * CHUNK_CLUSTER_HEIGHT) + y, (chunk->z * CHUNK_DEPTH) + z)
		   : cluster->blocks[x][y][z];
}

enum AOSides { AOSide_Top = BIT(4 + 0), AOSide_Bottom = BIT(4 + 1), AOSide_Left = BIT(4 + 2), AOSide_Right = BIT(4 + 3) };

const int aoTable[Directions_Count][4][3] = {
    {{0, 1, 0}, {0, -1, 0}, {-1, 0, 0}, {1, 0, 0}}, {{0, 1, 0}, {0, -1, 0}, {-1, 0, 0}, {1, 0, 0}}, {{0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}},
    {{0, 1, 0}, {0, -1, 0}, {0, 0, -1}, {0, 0, 1}}, {{0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}}, {{0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}},
};

typedef struct {
	Block blockType;
	uint8_t faceAO;
} VoxelFace;

static inline bool VoxelFaces_Equal(VoxelFace a, VoxelFace b) { return a.blockType == b.blockType && a.faceAO >> 4 == b.faceAO >> 4; }
static inline VoxelFace VoxelFace_Get(Cluster* cluster, int x, int y, int z, Direction side) { return (VoxelFace){cluster->blocks[x][y][z], side}; }

static vec_t(world_vertex) vertexlist;
static VoxelFace* mask;

void BlockRender_Init() {
	vec_init(&vertexlist);
	mask = malloc(sizeof(VoxelFace) * CHUNK_WIDTH * CHUNK_DEPTH);
}

void BlockRender_Free() {
	vec_deinit(&vertexlist);
	free(mask);
}

bool BlockRender_PolygonizeChunk(World* world, Chunk* chunk) {
	if (chunk->flags & ClusterFlags_InProcess) return false;

	for (Cluster* cluster = chunk->data; cluster < chunk->data + CHUNK_CLUSTER_COUNT; cluster++) {
		if (cluster->flags & ClusterFlags_VBODirty) {
			vec_clear(&vertexlist);

			memset(mask, 0, sizeof(VoxelFace) * CHUNK_WIDTH * CHUNK_DEPTH);

			int i, j, k, l, w, h, u, v, n, side = 0;

			int x[] = {0, 0, 0};
			int q[] = {0, 0, 0};
			int du[] = {0, 0, 0};
			int dv[] = {0, 0, 0};

			/*
			 * We create a mask - this will contain the groups of matching voxel faces
			 * as we proceed through the chunk in 6 directions - once for each face.
			 */

			/*
			 * These are just working variables to hold two faces during comparison.
			 */
			VoxelFace voxelFace, voxelFace1;

			/**
			 * We start with the lesser-spotted boolean for-loop (also known as the old flippy floppy).
			 *
			 * The variable backFace will be TRUE on the first iteration and FALSE on the second - this allows
			 * us to track which direction the indices should run during creation of the quad.
			 *
			 * This loop runs twice, and the inner loop 3 times - totally 6 iterations - one for each
			 * voxel face.
			 */
			for (bool backFace = true, b = false; b != backFace; backFace = backFace && b, b = !b) {
				/*
				 * We sweep over the 3 dimensions - most of what follows is well described by Mikola Lysenko
				 * in his post - and is ported from his Javascript implementation.  Where this implementation
				 * diverges, I've added commentary.
				 */
				for (int d = 0; d < 3; d++) {
					u = (d + 1) % 3;
					v = (d + 2) % 3;

					x[0] = 0;
					x[1] = 0;
					x[2] = 0;

					q[0] = 0;
					q[1] = 0;
					q[2] = 0;
					q[d] = 1;

					/*
					 * Here we're keeping track of the side that we're meshing.
					 */
					if (d == 0) {
						side = backFace ? Direction_Right : Direction_Left;
					} else if (d == 1) {
						side = backFace ? Direction_Bottom : Direction_Top;
					} else if (d == 2) {
						side = backFace ? Direction_Front : Direction_Back;
					}

					/*
					 * We move through the dimension from front to back
					 */
					for (x[d] = -1; x[d] < CHUNK_WIDTH;) {
						/*
						 * -------------------------------------------------------------------
						 *   We compute the mask
						 * -------------------------------------------------------------------
						 */
						n = 0;

						for (x[v] = 0; x[v] < CHUNK_CLUSTER_HEIGHT; x[v]++) {
							for (x[u] = 0; x[u] < CHUNK_WIDTH; x[u]++) {
								/*
								 * Here we retrieve two voxel faces for comparison.
								 */
								voxelFace = (x[d] >= 0) ? VoxelFace_Get(cluster, x[0], x[1], x[2], side) : (VoxelFace){0, 0};
								voxelFace1 = (x[d] < CHUNK_WIDTH - 1) ? VoxelFace_Get(cluster, x[0] + q[0], x[1] + q[1], x[2] + q[2], side)
												      : (VoxelFace){0, 0};

								/*
								 * Note that we're using the equals function in the voxel face class here, which lets the faces
								 * be compared based on any number of attributes.
								 *
								 * Also, we choose the face to add to the mask depending on whether we're moving through on a backface or not.
								 */
								mask[n++] = ((voxelFace.blockType != 0 && voxelFace.blockType != 0 && VoxelFaces_Equal(voxelFace, voxelFace1)))
										? (VoxelFace){0, 0}
										: backFace ? voxelFace1 : voxelFace;
							}
						}

						x[d]++;

						/*
						 * Now we generate the mesh for the mask
						 */
						n = 0;

						for (j = 0; j < CHUNK_CLUSTER_HEIGHT; j++) {
							for (i = 0; i < CHUNK_WIDTH;) {
								if (mask[n].blockType != 0) {
									/*
									 * We compute the width
									 */
									for (w = 1; i + w < CHUNK_WIDTH && mask[n + w].blockType != 0 && VoxelFaces_Equal(mask[n + w], mask[n]);
									     w++) {
									}

									/*
									 * Then we compute height
									 */
									bool done = false;

									for (h = 1; j + h < CHUNK_CLUSTER_HEIGHT; h++) {
										for (k = 0; k < w; k++) {
											if (mask[n + k + h * CHUNK_WIDTH].blockType != 0 ||
											    !VoxelFaces_Equal(mask[n + k + h * CHUNK_WIDTH], mask[n])) {
												done = true;
												break;
											}
										}

										if (done) {
											break;
										}
									}

									/*
									 * Here we check the "transparent" attribute in the VoxelFace class to ensure that we don't mesh
									 * any culled faces.
									 */
									if (/*!mask[n].transparent*/ true) {
										/*
										 * Add quad
										 */
										x[u] = i;
										x[v] = j;

										du[0] = 0;
										du[1] = 0;
										du[2] = 0;
										du[u] = w;

										dv[0] = 0;
										dv[1] = 0;
										dv[2] = 0;
										dv[v] = h;

										/*
										 * And here we call the quad function in order to render a merged quad in the scene.
										 *
										 * We pass mask[n] to the function, which is an instance of the VoxelFace class containing
										 * all the attributes of the face - which allows for variables to be passed to shaders - for
										 * example lighting values used to create ambient occlusion.
										 */

										C3D_FVec worldOffset =
										    FVec3(chunk->x * CHUNK_WIDTH, cluster->y * CHUNK_CLUSTER_HEIGHT, chunk->z * CHUNK_DEPTH);

										C3D_FVec verts[4];
										verts[0] = FVec_Add(FVec3(x[0], x[1], x[2]), worldOffset);			    // vorne links
										verts[1] = FVec_Add(FVec3(x[0] + du[0], x[1] + du[1], x[2] + du[2]), worldOffset);  // vorne rechts
										verts[2] = FVec_Add(
										    FVec3(x[0] + du[0] + dv[0], x[1] + du[1] + dv[1], x[2] + du[2] + dv[2]),  // hinten rechts
										    worldOffset);
										verts[3] = FVec_Add(FVec3(x[0] + dv[0], x[1] + dv[1], x[2] + dv[2]), worldOffset);  // hinten links

										const int vertOrder[2][6] = {{0, 1, 2, 3, 0, 2}, {0, 3, 2, 1, 0, 2}};

										uint8_t brightness[4] = {255, 255, 255, 255};

										world_vertex vtx;
										vtx.offset[0] = worldOffset.x;
										vtx.offset[1] = worldOffset.z;

										vtx.brightness[0] = 255;
										vtx.brightness[1] = 255;
										vtx.brightness[2] = 255;
										vtx.brightness[3] = 255;

										vtx.uvOffset[0] = 0;
										vtx.uvOffset[1] = 0;

										for (l = 0; l < 3; l++) {
											vtx.pointA[l] = x[l];
											vtx.pointB[l] = x[l] + du[l] + dv[l];
										}
										vtx.pointA[1] += (uint8_t)worldOffset.y;
										vtx.pointB[1] += (uint8_t)worldOffset.y;

										vec_push(&vertexlist, vtx);
									}

									/*
									 * We zero out the mask
									 */
									for (l = 0; l < h; ++l) {
										for (k = 0; k < w; ++k) {
											mask[n + k + l * CHUNK_WIDTH] = (VoxelFace){0};
										}
									}

									/*
									 * And then finally increment the counters and continue
									 */
									i += w;
									n += w;

								} else {
									i++;
									n++;
								}
							}
						}
					}
				}
			}

			cluster->vertexCount = vertexlist.length;

			if (!cluster->vertexCount) {
				printf("Empty cluster\n");
				cluster->flags &= ~ClusterFlags_VBODirty;
				continue;
			}
			if (cluster->vertexCount > 6) {
				printf("Cluster Vtx >6 %d, %d, %d\n", chunk->x, cluster->y, chunk->z);
			}

			int vboSizeMinimum = vertexlist.length * sizeof(world_vertex);
			if (!cluster->vbo || cluster->vboSize == 0) {
				cluster->vbo = linearAlloc(vboSizeMinimum + 64 * sizeof(world_vertex));
				cluster->vboSize = vboSizeMinimum + 64 * sizeof(world_vertex);
			} else if (cluster->vboSize < vboSizeMinimum) {
				linearFree(cluster->vbo);
				cluster->vbo = linearAlloc(vboSizeMinimum + 64 * sizeof(world_vertex));
				cluster->vboSize = vboSizeMinimum + 64 * sizeof(world_vertex);
			}
			memcpy(cluster->vbo, vertexlist.data, vboSizeMinimum);
			printf("Made %d vertices\n", vertexlist.length);

			cluster->flags &= ~ClusterFlags_VBODirty;
		}
	}

	vec_deinit(&vertexlist);

	return true;
}
