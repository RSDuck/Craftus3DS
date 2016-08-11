#include "craftus/render/render.h"

#include "craftus/render/vecmath.h"

#include <citro3d.h>

#include <stdio.h>

#include "vec/vec.h"

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

static const uint8_t diagTable[Directions_Count][3] = {{1, 1, 0}, {1, 1, 0}, {0, 1, 1}, {0, 1, 1}, {1, 0, 1}, {1, 0, 1}};
static const uint8_t sideOffset[Directions_Count][3] = {{0, 0, 0}, {0, 0, -1}, {1, 0, 0}, {0, 0, 0}, {1, 0, 0}, {0, 0, 0}};

static vec_t(world_vertex) tmpSides;

void BlockRender_Init() { vec_init(&tmpSides); }

void BlockRender_Free() { vec_deinit(&tmpSides); }

bool BlockRender_PolygonizeChunk(World* world, Chunk* chunk) {
	if (chunk->flags & ClusterFlags_InProcess) return false;

	int16_t chunkOffX = chunk->x * CHUNK_WIDTH;
	int16_t chunkOffZ = chunk->z * CHUNK_DEPTH;
	for (Cluster* cluster = chunk->data; cluster < chunk->data + CHUNK_CLUSTER_COUNT; cluster++) {
		if (cluster->flags & ClusterFlags_VBODirty) {
			vec_clear(&tmpSides);
			uint8_t clusterY = cluster->y * CHUNK_CLUSTER_HEIGHT;

			world_vertex side;
			for (int x = 0; x < CHUNK_WIDTH; x++)
				for (int z = 0; z < CHUNK_DEPTH; z++)
					for (int y = 0; y < CHUNK_CLUSTER_HEIGHT; y++) {
						if (cluster->blocks[x][y][z] != Block_Air) {
							for (int j = 0; j < Directions_Count; j++) {
								const int* pos = DirectionToPosition[j];
								const uint8_t* diag = diagTable[j];
								const uint8_t* sideOff = sideOffset[j];
								int backface = ~(pos[0] + pos[1] + pos[2]) + 1;

								if (fastBlockFetch(world, chunk, cluster, x + pos[0], y + pos[1], z + pos[2]) == Block_Air) {
									side = (world_vertex){
									    chunkOffX,
									    chunkOffZ,
									    {x + sideOff[0], y + clusterY + sideOff[1], z + sideOff[2]},
									    {x + diag[0] + sideOff[0], y + clusterY + diag[1] + sideOff[1], z + diag[2] + sideOff[2]},
									    {0, 0},
									    {255, 255, 255, 255},
									    {backface, 0, 0, 0}};

									// sides[sideCurrent] = (Side){x, y, z, j, cluster->blocks[x][y][z]};
									for (int k = 0; k < 4; k++) {
										const int* aoOffset = aoTable[j][k];
										if (fastBlockFetch(world, chunk, cluster, x + pos[0] + aoOffset[0], y + pos[1] + aoOffset[1],
												   z + pos[2] + aoOffset[2]) != Block_Air) {
											side.brightness[k] -= 20;
										}
									}
									vec_push(&tmpSides, side);
								}
							}
						}
					}

			int vboBytestNeeded = sizeof(world_vertex) * tmpSides.length;
			if (!vboBytestNeeded) {
				cluster->vertexCount = 0;
				cluster->flags = (cluster->flags & ~ClusterFlags_VBODirty) | ClusterFlags_Empty;
				continue;
			}

			if (cluster->vbo == NULL || cluster->vboSize == 0) {
				cluster->vbo = linearAlloc(vboBytestNeeded + (sizeof(world_vertex) * 16));
				cluster->vboSize = vboBytestNeeded;
			} else if (cluster->vboSize < vboBytestNeeded) {
				linearFree(cluster->vbo);
				cluster->vbo = linearAlloc(vboBytestNeeded + (sizeof(world_vertex) * 24));
				cluster->vboSize = vboBytestNeeded;
			}
			if (!cluster->vbo) printf("VBO allocation failed\n");

			memcpy(cluster->vbo, tmpSides.data, vboBytestNeeded);

			cluster->vertexCount = tmpSides.length;
			chunk->vertexCount += tmpSides.length;

			cluster->flags &= ~ClusterFlags_VBODirty;
		}
	}

	return true;
}
