#ifndef WORLD_H_INCLUDED
#define WORLD_H_INCLUDED

#include <stdint.h>
#include <stdbool.h>

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

enum { Cluster_Dirty = 0xFF & ~BIT(1), Cluster_DirtyVBO = BIT(0), Cluster_DirtyLocked = BIT(1) };

typedef struct {
	int y;

	Block blocks[CHUNK_WIDTH][CHUNK_CLUSTER_HEIGHT][CHUNK_DEPTH];
	int dirty;

	float* vbo;
	int vboSize;

	int vertexCount;
} Cluster;

typedef struct {
	int x, z;
	int referenced;

	int vertexCount;

	int dirty;

	Cluster data[CHUNK_HEIGHT / CHUNK_CLUSTER_HEIGHT];
} Chunk;

inline int Chunk_GetLocalX(int x) {
	int modX = (x - (x / CHUNK_WIDTH * CHUNK_WIDTH));
	return modX < 0 ? modX + CHUNK_WIDTH : modX;
}
inline int Chunk_GetLocalZ(int z) {
	int modZ = (z - (z / CHUNK_DEPTH * CHUNK_DEPTH));
	return modZ < 0 ? modZ + CHUNK_DEPTH : modZ;
}

inline int FastFloor(float x) { return (int)x - (x < (int)x); }

inline void Chunk_SetBlock(Chunk* c, int x, int y, int z, Block block) {
	c->data[y / CHUNK_CLUSTER_HEIGHT].blocks[x][y - (y / CHUNK_CLUSTER_HEIGHT * CHUNK_CLUSTER_HEIGHT)][z] = block;
	c->data[y / CHUNK_CLUSTER_HEIGHT].dirty = Cluster_DirtyVBO;
	c->dirty = Cluster_Dirty;
}

inline Block Chunk_GetBlock(Chunk* c, int x, int y, int z) { return c->data[y / CHUNK_CLUSTER_HEIGHT].blocks[x][y - (y / CHUNK_CLUSTER_HEIGHT * CHUNK_CLUSTER_HEIGHT)][z]; }

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

enum World_ErrorFlags { World_ErrUnloadedBlockRequested = 1, World_ErrLockedBlockRequested = 2 };

enum World_GenType { World_GenNormal = 0, World_GenSuperFlat = 1 };

typedef struct {
	unsigned long seed;
	int type;
} World_GenConfig;

typedef struct {
	char name[12];
	World_GenConfig genConfig;

	ChunkCache* cache[1];
	pool chunkpool;
	ChunkVec loadedChunks;

	int errFlags;
} World;

World* World_New();
void World_Free(World* world);

void World_Tick(World* world);

void World_SetBlock(World* world, int x, int y, int z, Block block);
Block World_GetBlock(World* world, int x, int y, int z);

Chunk* World_GetChunk(World* world, int x, int z);
void World_UnloadChunk(World* world, int x, int z);

void World_Profile(World* world);

#endif  // !WORLD_H_INCLUDED