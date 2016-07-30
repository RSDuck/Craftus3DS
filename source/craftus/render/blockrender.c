#include "craftus/render/render.h"

#include <citro3d.h>

#include <stdio.h>

static const world_vertex cube_sides_lut[] = {
    // First face (PZ)
    // First triangle
    {{-0.5f, -0.5f, +0.5f}, {0.0f, 0.0f}, {255, 255, 255, 255}},
    {{+0.5f, -0.5f, +0.5f}, {1.0f, 0.0f}, {255, 255, 255, 255}},
    {{+0.5f, +0.5f, +0.5f}, {1.0f, 1.0f}, {255, 255, 255, 255}},
    // Second triangle
    {{+0.5f, +0.5f, +0.5f}, {1.0f, 1.0f}, {255, 255, 255, 255}},
    {{-0.5f, +0.5f, +0.5f}, {0.0f, 1.0f}, {255, 255, 255, 255}},
    {{-0.5f, -0.5f, +0.5f}, {0.0f, 0.0f}, {255, 255, 255, 255}},

    // Second face (MZ)
    // First triangle
    {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f}, {255, 255, 255, 255}},
    {{-0.5f, +0.5f, -0.5f}, {1.0f, 1.0f}, {255, 255, 255, 255}},
    {{+0.5f, +0.5f, -0.5f}, {0.0f, 1.0f}, {255, 255, 255, 255}},
    // Second triangle
    {{+0.5f, +0.5f, -0.5f}, {0.0f, 1.0f}, {255, 255, 255, 255}},
    {{+0.5f, -0.5f, -0.5f}, {0.0f, 0.0f}, {255, 255, 255, 255}},
    {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f}, {255, 255, 255, 255}},

    // Third face (PX)
    // First triangle
    {{+0.5f, -0.5f, -0.5f}, {1.0f, 0.0f}, {255, 255, 255, 255}},
    {{+0.5f, +0.5f, -0.5f}, {1.0f, 1.0f}, {255, 255, 255, 255}},
    {{+0.5f, +0.5f, +0.5f}, {0.0f, 1.0f}, {255, 255, 255, 255}},
    // Second triangle
    {{+0.5f, +0.5f, +0.5f}, {0.0f, 1.0f}, {255, 255, 255, 255}},
    {{+0.5f, -0.5f, +0.5f}, {0.0f, 0.0f}, {255, 255, 255, 255}},
    {{+0.5f, -0.5f, -0.5f}, {1.0f, 0.0f}, {255, 255, 255, 255}},

    // Fourth face (MX)
    // First triangle
    {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f}, {255, 255, 255, 255}},
    {{-0.5f, -0.5f, +0.5f}, {1.0f, 0.0f}, {255, 255, 255, 255}},
    {{-0.5f, +0.5f, +0.5f}, {1.0f, 1.0f}, {255, 255, 255, 255}},
    // Second triangle
    {{-0.5f, +0.5f, +0.5f}, {1.0f, 1.0f}, {255, 255, 255, 255}},
    {{-0.5f, +0.5f, -0.5f}, {0.0f, 1.0f}, {255, 255, 255, 255}},
    {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f}, {255, 255, 255, 255}},

    // Fifth face (PY)
    // First triangle
    {{-0.5f, +0.5f, -0.5f}, {0.0f, 1.0f}, {255, 255, 255, 255}},
    {{-0.5f, +0.5f, +0.5f}, {0.0f, 0.0f}, {255, 255, 255, 255}},
    {{+0.5f, +0.5f, +0.5f}, {1.0f, 0.0f}, {255, 255, 255, 255}},
    // Second triangle
    {{+0.5f, +0.5f, +0.5f}, {1.0f, 0.0f}, {255, 255, 255, 255}},
    {{+0.5f, +0.5f, -0.5f}, {1.0f, 1.0f}, {255, 255, 255, 255}},
    {{-0.5f, +0.5f, -0.5f}, {0.0f, 1.0f}, {255, 255, 255, 255}},

    // Sixth face (MY)
    // First triangle
    {{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f}, {255, 255, 255, 255}},
    {{+0.5f, -0.5f, -0.5f}, {1.0f, 1.0f}, {255, 255, 255, 255}},
    {{+0.5f, -0.5f, +0.5f}, {1.0f, 0.0f}, {255, 255, 255, 255}},
    // Second triangle
    {{+0.5f, -0.5f, +0.5f}, {1.0f, 0.0f}, {255, 255, 255, 255}},
    {{-0.5f, -0.5f, +0.5f}, {0.0f, 0.0f}, {255, 255, 255, 255}},
    {{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f}, {255, 255, 255, 255}},
};

typedef struct {
	u8 x, y, z, sideAO;
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

bool BlockRender_PolygonizeChunk(World* world, Chunk* chunk) {
	if (chunk->flags & ClusterFlags_InProcess) return false;

	for (int i = 0; i < CHUNK_CLUSTER_COUNT; i++) {
		Cluster* cluster = &chunk->data[i];
		if (cluster->flags & ClusterFlags_VBODirty) {
			chunk->vertexCount -= cluster->vertexCount;

#define MAX_SIDES (6 * (CHUNK_WIDTH * CHUNK_CLUSTER_HEIGHT * CHUNK_DEPTH / 2))
			static Side sides[MAX_SIDES];
			int sideCurrent = 0;

			for (int x = 0; x < CHUNK_WIDTH; x++)
				for (int z = 0; z < CHUNK_DEPTH; z++)
					for (int y = 0; y < CHUNK_CLUSTER_HEIGHT; y++) {
						if (cluster->blocks[x][y][z] != Block_Air) {
							for (int j = 0; j < Directions_Count; j++) {
								const int* pos = DirectionToPosition[j];

								if (fastBlockFetch(world, chunk, cluster, x + pos[0], y + pos[1], z + pos[2]) == Block_Air) {
									sides[sideCurrent] = (Side){x, y, z, j, cluster->blocks[x][y][z]};

									for (int k = 0; k < 4; k++) {
										const int* aoOffset = aoTable[j][k];
										if (fastBlockFetch(world, chunk, cluster, x + pos[0] + aoOffset[0], y + pos[1] + aoOffset[1],
												   z + pos[2] + aoOffset[2]) != Block_Air) {
											sides[sideCurrent].sideAO |= 1 << (4 + k);
											// printf("AO Side %d, %d, %d\n", x, y, z);
										}
									}
									sideCurrent++;
								}
							}
						}
					}

			int vboBytestNeeded = sizeof(world_vertex) * 6 * sideCurrent;
			if (!vboBytestNeeded) {
				cluster->vertexCount = 0;
				cluster->flags &= ~ClusterFlags_VBODirty;
				cluster->flags |= ClusterFlags_Empty;
				continue;
			}

			if (cluster->vbo == NULL || cluster->vboSize == 0) {
				// printf("%d Allocating lin heap\n", chunk->x + chunk->z + cluster->y);
				cluster->vbo = linearAlloc(vboBytestNeeded + (sizeof(world_vertex) * 64));
				cluster->vboSize = vboBytestNeeded;
			} else if (cluster->vboSize < vboBytestNeeded) {
				// printf("%d Freeing and then allocating lin heap\n", chunk->x + chunk->z + cluster->y);
				linearFree(cluster->vbo);
				cluster->vbo = linearAlloc(vboBytestNeeded);
				cluster->vboSize = vboBytestNeeded;
			}
			if (!cluster->vbo) printf("VBO allocation failed\n");

			int vertexCount = 0;

			world_vertex* ptr = (world_vertex*)cluster->vbo;

			const float oneDivIconsPerRow = 1.f / 4.f;
			const float halfTexel = /*1.f / (64.f * 8.f)*/ 1.f / 128.f;

			float blockUV[2];
			for (int j = 0; j < sideCurrent; j++) {
				memcpy(ptr, &cube_sides_lut[(sides[j].sideAO & 0xF) * 6], sizeof(world_vertex) * 6);

				Blocks_GetUVs(sides[j].block, (sides[j].sideAO & 0xF), blockUV);

				int top = sides[j].sideAO & AOSide_Top;
				int bottom = sides[j].sideAO & AOSide_Bottom;
				int left = sides[j].sideAO & AOSide_Left;
				int right = sides[j].sideAO & AOSide_Right;

				float sideX = sides[j].x;
				float sideY = sides[j].y + (cluster->y * CHUNK_CLUSTER_HEIGHT);
				float sideZ = sides[j].z;

				if (i > 0) printf("%f\n", sideY);

				for (int k = 0; k < 6; k++) {
					ptr[k].position[0] += sideX;
					ptr[k].position[1] += sideY;
					ptr[k].position[2] += sideZ;

					int ao = 0;
					ao |= (ptr[k].uv[0] == 0.f && left);
					ao |= (ptr[k].uv[0] == 1.f && right);
					ao |= (ptr[k].uv[1] == 0.f && bottom);
					ao |= (ptr[k].uv[1] == 1.f && top);
					if (ao) {
						// printf("AO!\n");
						ptr[k].color[0] -= 55;
						ptr[k].color[1] -= 55;
						ptr[k].color[2] -= 55;
					}

					ptr[k].uv[0] = (ptr[k].uv[0] == 1.f ? (oneDivIconsPerRow - halfTexel) : halfTexel) + blockUV[0];
					ptr[k].uv[1] = (ptr[k].uv[1] == 1.f ? (oneDivIconsPerRow - halfTexel) : halfTexel) + blockUV[1];

					// printf("%f, %f\n", ptr[k].uv[0], ptr[k].uv[1]);
				}

				ptr += 6;
				vertexCount += 6;
			}
			// GX_FlushCacheRegions((u32*)cluster->vbo, (u32)cluster->vboSize, NULL, 0, NULL, 0);

			cluster->vertexCount = vertexCount;
			chunk->vertexCount += vertexCount;

			{
				FILE* f = fopen("vertdump.txt", "a");
				fprintf(f, "Cluster: %d %d %d Verts: %d\n", chunk->x, cluster->y, chunk->z, cluster->vertexCount);

				world_vertex* ptr = (world_vertex*)cluster->vbo;
				for (int j = 0; j < sideCurrent; j++, ptr++) {
					fprintf(f, "%f, %f, %f\t\n", ptr->position[0], ptr->position[1], ptr->position[2]);
				}
				fclose(f);
			}

			cluster->flags &= ~ClusterFlags_VBODirty;
		}
	}
	return true;
}
