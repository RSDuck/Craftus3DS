#include "craftus/world/world.h"
#include "craftus/world/chunkworker.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <stdio.h>

#include <3ds/allocator/linear.h>
#include <3ds/types.h>

World* World_New() {
	World* world = (World*)malloc(sizeof(World));

	strcpy(world->name, "Test");

	world->genConfig.seed = 2811;  // My birthday
	world->genConfig.type = World_GenNormal;

	world->errFlags = 0;

	poolInitialize(&world->chunkpool, sizeof(Chunk), CACHE_SIZE * CACHE_SIZE);
	vec_init(&world->loadedChunks);

	for (int i = 0; i < CACHE_SIZE * 4; i++) world->afterLife[i] = NULL;

	return world;
}

void World_Free(World* world) {
	vec_deinit(&world->loadedChunks);
	poolFreePool(&world->chunkpool);

	free(world);
}

void World_Profile(World* world) {
	printf("\nWorld: \n");
	printf("Cache Begin: %d, %d | Real Begin: %d, %d\n", world->cache[0]->where.startX, world->cache[0]->where.startY, world->cache[0]->cache[0][0]->x,
	       world->cache[0]->cache[0][0]->z);
	printf("GC Cache Capacity: %d | Length %d\n", world->loadedChunks.capacity, world->loadedChunks.length);
}

void World_EnterAfterLive() {}

extern ChunkWorker* cworker;
Chunk* World_GetChunk(World* world, int x, int z) {
	Chunk* chunk;
	int i;
	vec_foreach (&world->loadedChunks, chunk, i)
		if (chunk->x == x && chunk->z == z) {
			chunk->referenced++;
			return chunk;
		}
	// printf("No chunk loaded\n");
	for (i = 0; i < CACHE_SIZE * 4; i++)
		if (world->afterLife[i] != NULL)
			if (world->afterLife[i]->x == x && world->afterLife[i]->z == z) {
				Chunk* chunk = world->afterLife[i];
				world->afterLife[i] = NULL;
				chunk->referenced = 1;
				// printf("Fetched chunk from afterlife\n");
				return chunk;
			}

	// printf("Making a new chunk\n");

	// Make a new one
	chunk = poolMalloc(&world->chunkpool);
	memset(chunk, 0, sizeof(Chunk));
	chunk->x = x, chunk->z = z, chunk->referenced = 1;
	for (int i = 0; i < CHUNK_CLUSTER_COUNT; i++) {
		chunk->data[i].y = i;
	}

	vec_push(&world->loadedChunks, chunk);

	ChunkWorker_AddJob(cworker, chunk, ChunkWorker_TaskDecorateChunk);

	return chunk;
}

void World_FreeChunk(World* world, Chunk* chunk) {
	for (int j = 0; j < CHUNK_CLUSTER_COUNT; j++) linearFree(chunk->data[j].vbo);
	poolFree(&world->chunkpool, chunk);
}

void World_ReleaseChunk(World* world, int x, int z) {
	Chunk* chunk;
	int i;
	vec_foreach (&world->loadedChunks, chunk, i)
		if (chunk->x == x && chunk->z == z)
			if (--chunk->referenced == 0) {
				vec_splice(&world->loadedChunks, i, 1);
				for (int j = 0; j < CACHE_SIZE * 4; j++) {
					if (world->afterLife[j] == NULL) {
						world->afterLife[j] = chunk;
						// printf("Put chunk into afterlife\n");
						break;
					}
				}

				return;
			}
}

void World_ClearAfterLife(World* world) {
	for (int i = 0; i < CACHE_SIZE * 2; i++) {
		if (world->afterLife[i] != NULL) {
			if (world->afterLife[i]->referenced < -2) {  // -2 ist richtig
				if (!(world->afterLife[i]->flags & ClusterFlags_InProcess)) World_FreeChunk(world, world->afterLife[i]);
				world->afterLife[i] = NULL;
				// printf("Freed chunk in after life\n");
			} else {
				world->afterLife[i]->referenced--;
				// printf("Decreased chunk counter in afterlife %d\n", world->afterLife[i]->referenced);
			}
		}
	}
}

void World_Tick(World* world) {
	ChunkCache* cache = world->cache[0];
	int orginalX = cache->cache[0][0]->x, orginalZ = cache->cache[0][0]->z;
	if (cache->updatedWhere.startX != orginalX || cache->updatedWhere.startY != orginalZ) {
		int delta = cache->updatedWhere.startX - cache->cache[0][0]->x;
		int axis = 0;

#define CACHE_AXIS cache->cache[!axis ? i : j][!axis ? j : i]

		do {
			if (delta != 0) {
				if (delta > 0)
					for (int i = 0; i < CACHE_SIZE; i++) {
						if (i == 0)
							for (int j = 0; j < CACHE_SIZE; j++) World_ReleaseChunk(world, CACHE_AXIS->x, CACHE_AXIS->z);
						if (i + delta >= CACHE_SIZE)
							for (int j = 0; j < CACHE_SIZE; j++)
								CACHE_AXIS = World_GetChunk(world, CACHE_AXIS->x + (!axis ? delta : 0), CACHE_AXIS->z + (!axis ? 0 : delta));
						else
							for (int j = 0; j < CACHE_SIZE; j++) CACHE_AXIS = cache->cache[!axis ? (i + delta) : j][!axis ? j : (i + delta)];
					}
				else
					for (int i = CACHE_SIZE - 1; i >= 0; i--) {
						if (i == CACHE_SIZE - 1)
							for (int j = 0; j < CACHE_SIZE; j++) World_ReleaseChunk(world, CACHE_AXIS->x, CACHE_AXIS->z);
						if (i + delta < 0)
							for (int j = 0; j < CACHE_SIZE; j++)
								CACHE_AXIS = World_GetChunk(world, CACHE_AXIS->x + (!axis ? delta : 0), CACHE_AXIS->z + (!axis ? 0 : delta));
						else
							for (int j = 0; j < CACHE_SIZE; j++) CACHE_AXIS = cache->cache[!axis ? (i + delta) : j][!axis ? j : (i + delta)];
					}
			}

			delta = cache->updatedWhere.startY - cache->cache[0][0]->z;
			orginalX = cache->cache[0][0]->x, orginalZ = cache->cache[0][0]->z;
		} while (++axis < 2);

#undef CACHE_AXIS

		cache->where = cache->updatedWhere;

		World_ClearAfterLife(world);
	}
}

void World_SetBlock(World* world, int x, int y, int z, Block block) {
	ChunkCache* cache = world->cache[0];
	if (Box_IsPointInside(cache->where, BlockToChunkCoordZ(x), BlockToChunkCoordZ(z)) && y >= 0 && y < CHUNK_HEIGHT) {
		int chunkX = ChunkCache_LocalizeChunkX(cache, x), chunkZ = ChunkCache_LocalizeChunkZ(cache, z);
		int lX = Chunk_GetLocalX(x), lZ = Chunk_GetLocalZ(z);

		world->errFlags &= ~(World_ErrUnloadedBlockRequested);
		if (cache->cache[chunkX][chunkZ]->flags & ClusterFlags_InProcess) {
			world->errFlags |= World_ErrLockedBlockRequested;
			return;
		}
		world->errFlags &= ~World_ErrLockedBlockRequested;
		Chunk_SetBlock(cache->cache[chunkX][chunkZ], lX, y, lZ, block);

		int clusterY = y / CHUNK_CLUSTER_HEIGHT;
		if (block != Block_Air) cache->cache[chunkX][chunkZ]->data[clusterY].flags &= ~ClusterFlags_Empty;

		if (lX == 0 && chunkX - 1 >= 0) Chunk_MarkCluster(cache->cache[chunkX - 1][chunkZ], clusterY, ClusterFlags_VBODirty);
		if (lX == CHUNK_WIDTH - 1 && chunkX + 1 < CACHE_SIZE) Chunk_MarkCluster(cache->cache[chunkX + 1][chunkZ], clusterY, ClusterFlags_VBODirty);
		if (lZ == 0 && chunkZ - 1 >= 0) Chunk_MarkCluster(cache->cache[chunkX][chunkZ - 1], clusterY, ClusterFlags_VBODirty);
		if (lZ == CHUNK_DEPTH - 1 && chunkZ + 1 < CACHE_SIZE) Chunk_MarkCluster(cache->cache[chunkX][chunkZ + 1], clusterY, ClusterFlags_VBODirty);
		int lY = y - (y / CHUNK_CLUSTER_HEIGHT * CHUNK_CLUSTER_HEIGHT);
		if (lY == 0 && clusterY - 1 >= 0) Chunk_MarkCluster(cache->cache[chunkX][chunkZ], clusterY - 1, ClusterFlags_VBODirty);
		if (lY == CHUNK_CLUSTER_HEIGHT - 1 && clusterY < CHUNK_CLUSTER_HEIGHT) Chunk_MarkCluster(cache->cache[chunkX][chunkZ], clusterY + 1, ClusterFlags_VBODirty);

		return;
	}
	world->errFlags |= World_ErrUnloadedBlockRequested;
}

Block World_GetBlock(World* world, int x, int y, int z) {
	ChunkCache* cache = world->cache[0];
	if (Box_IsPointInside(cache->where, BlockToChunkCoordZ(x), BlockToChunkCoordZ(z)) && y >= 0 && y < CHUNK_HEIGHT) {
		int chunkX = ChunkCache_LocalizeChunkX(cache, x), chunkZ = ChunkCache_LocalizeChunkZ(cache, z);
		world->errFlags &= ~World_ErrUnloadedBlockRequested;
		if (cache->cache[chunkX][chunkZ]->flags & ClusterFlags_InProcess) {
			world->errFlags |= World_ErrLockedBlockRequested;
			return Block_Air;
		}
		world->errFlags &= ~(World_ErrLockedBlockRequested);
		return Chunk_GetBlock(cache->cache[chunkX][chunkZ], Chunk_GetLocalX(x), y, Chunk_GetLocalZ(z));
	}
	world->errFlags |= World_ErrUnloadedBlockRequested;
	return Block_Air;
}

void Chunk_RecalcHeightMap(Chunk* chunk) {
	for (int x = 0; x < CHUNK_WIDTH; x++) {
		for (int z = 0; z < CHUNK_DEPTH; z++) {
			for (int y = CHUNK_HEIGHT - 1; y >= 0; y--) {
				if (chunk->data[y / CHUNK_CLUSTER_HEIGHT].flags & ClusterFlags_Empty) y -= (CHUNK_CLUSTER_HEIGHT - 1);
				if (Chunk_GetBlock(chunk, x, y, z) != Block_Air) {
					chunk->heightmap[x][z] = y;
					break;
				}
			}
		}
	}
}

Chunk* World_FastChunkAccess(World* world, int x, int z) {
	ChunkCache* cache = world->cache[0];
	if (Box_IsPointInside(cache->where, BlockToChunkCoordZ(x), BlockToChunkCoordZ(z))) {
		int chunkX = ChunkCache_LocalizeChunkX(cache, x), chunkZ = ChunkCache_LocalizeChunkZ(cache, z);

		world->errFlags &= ~World_ErrUnloadedBlockRequested;
		return cache->cache[chunkX][chunkZ];
	}
	world->errFlags |= World_ErrUnloadedBlockRequested;
	return NULL;
}