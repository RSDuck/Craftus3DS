#ifndef WORLD_H_INCLUDED
#define WORLD_H_INCLUDED

#include <stdbool.h>
#include <stdint.h>

#include <3ds/types.h>

#include "craftus/misc/box.h"
#include "craftus/world/blocks.h"

#include "pool/pool.h"
#include "vec/vec.h"

#define CHUNK_WIDTH 16
#define CHUNK_HEIGHT 128
#define CHUNK_DEPTH 16

#define CHUNK_CLUSTER_HEIGHT 16
#define CHUNK_CLUSTER_COUNT CHUNK_HEIGHT / CHUNK_CLUSTER_HEIGHT

enum { ClusterFlags_VBODirty = BIT(0), ClusterFlags_InProcess = BIT(1), ClusterFlags_Empty = BIT(2) };

typedef struct {
	int y;

	Block blocks[CHUNK_WIDTH][CHUNK_CLUSTER_HEIGHT][CHUNK_DEPTH];
	int flags;

	float* vbo;
	int vboSize;

	int vertexCount;
} Cluster;

typedef struct {
	int x, z;
	int referenced;

	u8 heightmap[CHUNK_WIDTH][CHUNK_DEPTH];

	int vertexCount;

	int flags;

	Cluster data[CHUNK_CLUSTER_COUNT];
} Chunk;

inline void Chunk_MarkCluster(Chunk* chunk, int cluster, int flag) {
	chunk->data[cluster].flags |= flag;
	chunk->flags |= flag;
}

inline int Chunk_GetLocalX(int x) {
	int modX = (x - (x / CHUNK_WIDTH * CHUNK_WIDTH));
	return modX < 0 ? modX + CHUNK_WIDTH : modX;
}
inline int Chunk_GetLocalZ(int z) {
	int modZ = (z - (z / CHUNK_DEPTH * CHUNK_DEPTH));
	return modZ < 0 ? modZ + CHUNK_DEPTH : modZ;
}

void Chunk_RecalcHeightMap(Chunk* chunk);

inline int FastFloor(float x) { return (int)x - (x < (int)x); }

inline void Chunk_SetBlock(Chunk* c, int x, int y, int z, Block block) {
	c->data[y / CHUNK_CLUSTER_HEIGHT].blocks[x][y - (y / CHUNK_CLUSTER_HEIGHT * CHUNK_CLUSTER_HEIGHT)][z] = block;
	c->data[y / CHUNK_CLUSTER_HEIGHT].flags |= ClusterFlags_VBODirty;
	c->flags |= ClusterFlags_VBODirty;
}

inline Block Chunk_GetBlock(Chunk* c, int x, int y, int z) {
	return c->data[y / CHUNK_CLUSTER_HEIGHT].blocks[x][y - (y / CHUNK_CLUSTER_HEIGHT * CHUNK_CLUSTER_HEIGHT)][z];
}

#define CACHE_SIZE 9

typedef struct {
	Chunk* cache[CACHE_SIZE][CACHE_SIZE];
	Box where;
	Box updatedWhere;
} ChunkCache;

inline int BlockToChunkCoordX(int x) { return FastFloor((float)x / CHUNK_WIDTH); }
inline int BlockToChunkCoordZ(int z) { return FastFloor((float)z / CHUNK_DEPTH); }

inline int ChunkCache_LocalizeChunkX(ChunkCache* cache, int x) { return BlockToChunkCoordX(x) - cache->cache[0][0]->x; }
inline int ChunkCache_LocalizeChunkZ(ChunkCache* cache, int z) { return BlockToChunkCoordZ(z) - cache->cache[0][0]->z; }

typedef vec_t(Chunk*) ChunkVec;

enum World_ErrorFlags { World_ErrUnloadedBlockRequested = BIT(0), World_ErrLockedBlockRequested = BIT(1) };

enum World_GenType { World_GenNormal = 0, World_GenSuperFlat = 1 };

typedef struct {
	long seed;
	int type;
} World_GenConfig;

#define CHUNK_AFTERLIFE_SIZE (CACHE_SIZE * 4)
typedef struct {
	char name[12];
	World_GenConfig genConfig;

	ChunkCache* cache[1];
	pool chunkpool;
	ChunkVec loadedChunks;

	Chunk* afterLife[CHUNK_AFTERLIFE_SIZE];

	int errFlags;

	uint32_t tickRandom;
} World;

World* World_New();
void World_Free(World* world);

void World_Tick(World* world);

void World_SetBlock(World* world, int x, int y, int z, Block block);
Block World_GetBlock(World* world, int x, int y, int z);

Chunk* World_GetChunk(World* world, int x, int z);
void World_UnloadChunk(World* world, int x, int z);

void World_Profile(World* world);

void World_FreeChunk(World* world, Chunk* chunk);

Chunk* World_FastChunkAccess(World* world, int x, int z);

int World_GetHeight(World* world, int x, int z);

#endif  // !WORLD_H_INCLUDED