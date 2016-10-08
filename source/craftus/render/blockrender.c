#include "craftus/render/render.h"

#include "craftus/render/vbocache.h"
#include "craftus/world/chunkworker.h"

#include <citro3d.h>

#include <stdio.h>

#include "vec/vec.h"

const world_vertex plantmodel[] = {
    // First face
    // Erstes Dreieck
    {{0, 0}, {0, 0, 0, 255}},
    {{1, 1}, {0, 1, 0, 255}},
    {{1, 1}, {1, 1, 1, 255}},
    // Zweites Dreieck
    {{1, 1}, {1, 1, 1, 255}},
    {{0, 0}, {1, 0, 1, 255}},
    {{0, 0}, {0, 0, 0, 255}},

    // Zweites face(Rückseite)
    // Erstes Dreieck
    {{1, 1}, {0, 1, 0, 255}},
    {{0, 0}, {0, 0, 0, 255}},
    {{1, 1}, {1, 1, 1, 255}},

    // Zweites Dreieck
    {{1, 1}, {1, 1, 1, 255}},
    {{0, 0}, {0, 0, 0, 255}},
    {{0, 0}, {1, 0, 1, 255}},

    // Drittes face
    // erstes Dreieck
    {{1, 0}, {0, 0, 0, 255}},
    {{0, 1}, {0, 1, 0, 255}},
    {{0, 1}, {1, 1, 1, 255}},
    // Zweites Dreieck
    {{0, 1}, {1, 1, 1, 255}},
    {{1, 0}, {1, 0, 1, 255}},
    {{0, 0}, {0, 0, 0, 255}},

    // Viertes face
    // erstes dreeck
    {{1, 0}, {0, 0, 0, 255}},
    {{0, 1}, {1, 1, 1, 255}},
    {{0, 1}, {0, 1, 0, 255}},

    // Zweites Dreieck
    {{0, 1}, {1, 1, 1, 255}},
    {{0, 0}, {0, 0, 0, 255}},
    {{1, 0}, {1, 0, 1, 255}},
};

const world_vertex cube_sides_lut[] = {
    // First face (PZ)
    // First triangle
    {{0, 1}, {0, 0, 0, 255}},
    {{1, 1}, {0, 1, 0, 255}},
    {{1, 1}, {1, 1, 1, 255}},
    // Second triangle
    {{1, 1}, {1, 1, 1, 255}},
    {{0, 1}, {1, 0, 1, 255}},
    {{0, 1}, {0, 0, 0, 255}},

    // Second face (MZ)
    // First triangle
    {{0, 0}, {0, 1, 0, 255}},
    {{0, 0}, {1, 1, 1, 255}},
    {{1, 0}, {1, 0, 1, 255}},
    // Second triangle
    {{1, 0}, {1, 0, 1, 255}},
    {{1, 0}, {0, 0, 0, 255}},
    {{0, 0}, {0, 1, 0, 255}},

    // Third face (PX)
    // First triangle
    {{1, 0}, {0, 1, 0, 255}},
    {{1, 0}, {1, 1, 1, 255}},
    {{1, 1}, {1, 0, 1, 255}},
    // Second triangle
    {{1, 1}, {1, 0, 1, 255}},
    {{1, 1}, {0, 0, 0, 255}},
    {{1, 0}, {0, 1, 0, 255}},

    // Fourth face (MX)
    // First triangle
    {{0, 0}, {0, 0, 0, 255}},
    {{0, 1}, {0, 1, 0, 255}},
    {{0, 1}, {1, 1, 1, 255}},
    // Second triangle
    {{0, 1}, {1, 1, 1, 255}},
    {{0, 0}, {1, 0, 1, 255}},
    {{0, 0}, {0, 0, 0, 255}},

    // Fifth face (PY)
    // First triangle
    {{0, 0}, {1, 0, 1, 255}},
    {{0, 1}, {1, 0, 0, 255}},
    {{1, 1}, {1, 1, 0, 255}},
    // Second triangle
    {{1, 1}, {1, 1, 0, 255}},
    {{1, 0}, {1, 1, 1, 255}},
    {{0, 0}, {1, 0, 1, 255}},

    // Sixth face (MY)
    // First triangle
    {{0, 0}, {0, 0, 1, 255}},
    {{1, 0}, {0, 1, 1, 255}},
    {{1, 1}, {0, 1, 0, 255}},
    // Second triangle
    {{1, 1}, {0, 1, 0, 255}},
    {{0, 1}, {0, 0, 0, 255}},
    {{0, 0}, {0, 0, 1, 255}},
};

typedef struct {
	u8 x, y, z, sideAO;
	Block block;
	u8 brightnes;
} Side;

static inline Block fastBlockFetch(World* world, Chunk* chunk, Cluster* cluster, int x, int y, int z) {
	world->errFlags = 0;
	return (x < 0 || y < 0 || z < 0 || x >= CHUNK_WIDTH || y >= CHUNK_CLUSTER_HEIGHT || z >= CHUNK_DEPTH)
		   ? World_GetBlock(world, (chunk->x * CHUNK_WIDTH) + x, (cluster->y * CHUNK_CLUSTER_HEIGHT) + y, (chunk->z * CHUNK_DEPTH) + z)
		   : cluster->blocks[x][y][z];
}

static inline u8 fastHeightMapFetch(World* world, Chunk* chunk, int x, int z) {
	return (x < 0 || z < 0 || x >= CHUNK_WIDTH || z >= CHUNK_DEPTH)
		   ? World_FastChunkAccess(world, chunk->x * CHUNK_WIDTH + x, chunk->z * CHUNK_DEPTH + z)
			 ->heightmap[Chunk_GetLocalX(chunk->x * CHUNK_WIDTH + x)][Chunk_GetLocalZ(chunk->z * CHUNK_DEPTH + z)]
		   : chunk->heightmap[x][z];
}

enum AOSides { AOSide_Top = BIT(4 + 0), AOSide_Bottom = BIT(4 + 1), AOSide_Left = BIT(4 + 2), AOSide_Right = BIT(4 + 3) };

const int aoTable[Directions_Count][4][3] = {
    {{0, 1, 0}, {0, -1, 0}, {-1, 0, 0}, {1, 0, 0}}, {{0, 1, 0}, {0, -1, 0}, {-1, 0, 0}, {1, 0, 0}}, {{0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}},
    {{0, 1, 0}, {0, -1, 0}, {0, 0, -1}, {0, 0, 1}}, {{0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}}, {{0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}},
};

#define MAX_SIDES (6 * (CHUNK_WIDTH * CHUNK_CLUSTER_HEIGHT * CHUNK_DEPTH / 2))

typedef struct {
	int x, y, z;
	int vtxCount[2];
	void* buf[2];
} pendingChunkVtxs;

#define BUFFERS_COUNT (2 * 4)
static void* buffers[BUFFERS_COUNT];
static int buffersUsed = 0;

static void* aquireBuffer() {
	if (buffersUsed != 0xFF) {
		for (int i = 0; i < BUFFERS_COUNT; i++) {
			if (!(buffersUsed & (1 << i))) {
				buffersUsed |= (1 << i);
				return buffers[i];
			}
		}
	}
	return NULL;
}

static void freeBuffer(void* buffer) {
	for (int i = 0; i < BUFFERS_COUNT; i++) {
		if (buffers[i] == buffer) {
			buffersUsed &= ~(1 << i);
			return;
		}
	}
}

static vec_t(pendingChunkVtxs) pendingChunks;

void BlockRender_Init() {
	vec_init(&pendingChunks);
	for (int i = 0; i < BUFFERS_COUNT; i++) buffers[i] = malloc(MAX_SIDES * sizeof(world_vertex) * 6);
}

void BlockRender_Free() {
	vec_deinit(&pendingChunks);
	for (int i = 0; i < BUFFERS_COUNT; i++) free(buffers[i]);
}

void BlockRender_PutBuffersIntoPlace(World* world) {
	if (pendingChunks.length > 0) {
		pendingChunkVtxs vtxData = vec_pop(&pendingChunks);
		Chunk* c = World_GetChunk(world, vtxData.x, vtxData.z);
		if (c != NULL) {
			Cluster* cluster = &c->data[vtxData.y];
			for (int i = 0; i < 2; i++) {
				if (vtxData.buf[i] != NULL) {
					int vboBytestNeeded = sizeof(world_vertex) * vtxData.vtxCount[i];
					if (cluster->vbo[i].memory == NULL || cluster->vbo[i].size == 0) {
						cluster->vbo[i] = VBO_Alloc(vboBytestNeeded + (sizeof(world_vertex) * 24));
					} else if (cluster->vbo[i].size < vboBytestNeeded) {
						VBO_Free(cluster->vbo[i]);
						cluster->vbo[i] = VBO_Alloc(vboBytestNeeded + (sizeof(world_vertex) * 24));
					}
					if (!cluster->vbo[i].memory) printf("VBO allocation failed\n");
					memcpy(cluster->vbo[i].memory, vtxData.buf[i], vboBytestNeeded);
					freeBuffer(vtxData.buf[i]);
				}
			}
			cluster->vertexCount[0] = vtxData.vtxCount[0];
			cluster->vertexCount[1] = vtxData.vtxCount[1];
		}
	}
}

bool BlockRender_TaskPolygonizeChunk(ChunkWorker_Queue* queue, ChunkWorker_Task task) {
	BlockRender_PolygonizeChunk(task.world, task.chunk, false);
	printf("Executing polytask\n");
	task.chunk->flags &= ~ClusterFlags_VBODirty;
	return true;
}

bool BlockRender_PolygonizeChunk(World* world, Chunk* chunk, bool progressive) {
	// if (chunk->tasksPending > 0) return false;

	int clean = 0;

	printf("Recalculating chunk\n");

	Chunk_RecalcHeightMap(chunk);

	for (int i = 0; i < CHUNK_CLUSTER_COUNT; i++) {
		Cluster* cluster = &chunk->data[i];

		pendingChunkVtxs vtx = {chunk->x, i, chunk->z, {0, 0}, {NULL, NULL}};
		if (cluster->flags & ClusterFlags_VBODirty) {
			static Side sideBuffers[2][MAX_SIDES];
			Side* sides = &sideBuffers[0][0];

			int sideCurrent = 0;
			int sidesCount[2] = {0, 0};  // Pass 0, 1

			int blocksNotAir = 0;

			for (int x = 0; x < CHUNK_WIDTH; x++)
				for (int z = 0; z < CHUNK_DEPTH; z++) {
					for (int y = 0; y < CHUNK_CLUSTER_HEIGHT; y++) {
						Block block = cluster->blocks[x][y][z];
						if (block != Block_Air) {
							blocksNotAir++;
							for (int j = 0; j < Directions_Count; j++) {
								const int* pos = &DirectionToPosition[j];

								Block sidedBlock = fastBlockFetch(world, chunk, cluster, x + pos[0], y + pos[1], z + pos[2]);
								if (sidedBlock == Block_Air || !Blocks_IsOpaque(sidedBlock)) {
									if (!(world->errFlags & World_ErrUnloadedBlockRequested)) {
										Side s = (Side){
										    x,
										    y,
										    z,
										    j,
										    block,
										    (i * CHUNK_CLUSTER_HEIGHT) + y < fastHeightMapFetch(world, chunk, x + pos[0], z + pos[2])};

										for (int k = 0; k < 4; k++) {
											const int* aoOffset = aoTable[j][k];
											if (fastBlockFetch(world, chunk, cluster, x + pos[0] + aoOffset[0],
													   y + pos[1] + aoOffset[1], z + pos[2] + aoOffset[2]) != Block_Air) {
												s.sideAO |= 1 << (4 + k);
											}
										}
										sides[sideCurrent++] = s;
										sidesCount[!Blocks_IsOpaque(block)]++;
									}
								}
							}
						}
					}
				}

			if (!sideCurrent) {
				/*cluster->vertexCount[0] = 0;
				cluster->vertexCount[1] = 0;*/
				vtx.vtxCount[0] = 0;
				vtx.vtxCount[1] = 0;
				cluster->flags &= ~ClusterFlags_VBODirty;
				if (blocksNotAir == 0) cluster->flags |= ClusterFlags_Empty;
				vec_push(&pendingChunks, vtx);
				continue;
			}

			for (int k = 0; k < 2; k++) {
				if (sidesCount[k]) {
					do {
						vtx.buf[k] = aquireBuffer();
						svcSleepThread(120);
					} while (vtx.buf[k] == NULL);
				}
			}

			int vertexCount[2] = {0, 0};

			world_vertex* vbos[2] = {vtx.buf[0], vtx.buf[1]};

			const int oneDivIconsPerRow = 256 / 8;
			const int halfTexel = 256 / 128;

			int16_t clusterX = (chunk->x * CHUNK_WIDTH);
			uint8_t clusterY = (cluster->y * CHUNK_CLUSTER_HEIGHT);
			int16_t clusterZ = (chunk->z * CHUNK_DEPTH);

			uint8_t blockUV[2];

			for (int j = 0; j < sideCurrent; j++) {
				bool blockTransparent = !Blocks_IsOpaque(sides[j].block);

				world_vertex* ptr = vbos[blockTransparent];

				memcpy(ptr, &cube_sides_lut[(sides[j].sideAO & 0xF) * 6], sizeof(world_vertex) * 6);

				Blocks_GetUVs(sides[j].block, (sides[j].sideAO & 0xF), blockUV);

				// if (i > 0) printf("%u %f %f\n", sides[j].block, blockUV[0], blockUV[1]);

				int top = sides[j].sideAO & AOSide_Top;
				int bottom = sides[j].sideAO & AOSide_Bottom;
				int left = sides[j].sideAO & AOSide_Left;
				int right = sides[j].sideAO & AOSide_Right;

				int16_t sideX = sides[j].x + clusterX;
				uint8_t sideY = sides[j].y + clusterY;
				int16_t sideZ = sides[j].z + clusterZ;

				for (int k = 0; k < 6; k++) {
					ptr[k].xz[0] += sideX;
					ptr[k].xz[1] += sideZ;
					ptr[k].yuvb[0] += sideY;

					int ao = 0;
					ao |= (ptr[k].yuvb[1] == 0 && left);
					ao |= (ptr[k].yuvb[1] == 1 && right);
					ao |= (ptr[k].yuvb[2] == 0 && bottom);
					ao |= (ptr[k].yuvb[2] == 1 && top);
					if (ao) {
						ptr[k].yuvb[3] -= 55;
					}
					if (sides[j].brightnes) {
						ptr[k].yuvb[3] -= 90;
					}

					ptr[k].yuvb[1] = (ptr[k].yuvb[1] == 1 ? (oneDivIconsPerRow - halfTexel) : halfTexel) + blockUV[0];
					ptr[k].yuvb[2] = (ptr[k].yuvb[2] == 1 ? (oneDivIconsPerRow - halfTexel) : halfTexel) + blockUV[1];

					// printf("%f, %f\n", ptr[k].uv[0], ptr[k].uv[1]);
				}

				vbos[blockTransparent] += 6;
				vertexCount[blockTransparent] += 6;
			}

			/*{
				FILE* f = fopen("chunk_sizes.txt", "a");
				fprintf(f, "%dkb|\t%d sides|\t%d blocks\n", vboBytestNeeded, sideCurrent, blocksNotAir);
				fclose(f);
			}*/

			vtx.vtxCount[0] = vertexCount[0];
			vtx.vtxCount[1] = vertexCount[1];

			vec_push(&pendingChunks, vtx);

			cluster->flags &= ~ClusterFlags_VBODirty;

			clean++;

			if (progressive) break;
		} else
			clean++;
	}

	printf("Finished\n");

	return clean == CHUNK_CLUSTER_COUNT;
}