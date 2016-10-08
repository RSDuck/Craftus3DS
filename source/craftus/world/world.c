#include "craftus/world/world.h"
#include "craftus/world/chunkworker.h"

#include "craftus/misc/random.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <stdio.h>

#include <3ds/allocator/linear.h>
#include <3ds/types.h>

static RecursiveLock worldLock;
World* World_New() {
	World* world = (World*)malloc(sizeof(World));

	strcpy(world->name, "Test");

	world->genConfig.seed = 2811;  // My birthday
	world->genConfig.type = World_GenNormal;

	world->errFlags = 0;

	world->tickRandom = 12345;

	poolInitialize(&world->chunkpool, sizeof(Chunk), CACHE_SIZE * CACHE_SIZE);
	vec_init(&world->loadedChunks);

	for (int i = 0; i < CHUNK_AFTERLIFE_SIZE; i++) world->afterLife[i] = NULL;

	RecursiveLock_Init(&worldLock);

	return world;
}

void World_PrepareFree(World* world) {
	while (true) {
		int notEmpty = 0;
		for (int i = 0; i < CHUNK_AFTERLIFE_SIZE; i++) {
			notEmpty += (int)world->afterLife[i];
		}
		if (!notEmpty) {
			break;
		}
		World_ClearAfterLife(world);
	}

	for (int i = 0; i < CACHE_SIZE; i++) {
		for (int j = 0; j < CACHE_SIZE; j++) {
			Chunk* chunk = world->cache[0]->cache[i][j];
			World_ReleaseChunk(world, chunk->x, chunk->z);
		}
	}
}

void World_Free(World* world) {
	RecursiveLock_Lock(&worldLock);
	vec_deinit(&world->loadedChunks);
	poolFreePool(&world->chunkpool);

	free(world);

	RecursiveLock_Unlock(&worldLock);
}

void World_Profile(World* world) {
	printf("\nWorld: \n");
	/*printf("Cache Begin: %d, %d | Real Begin: %d, %d\n", world->cache[0]->where.startX, world->cache[0]->where.startY,
	       world->cache[0]->cache[0][0]->x, world->cache[0]->cache[0][0]->z);*/
	int afterLifeUsage = 0;
	for (int i = 0; i < CHUNK_AFTERLIFE_SIZE; i++) afterLifeUsage += !!world->afterLife[i];
	printf("GC Cache Capacity: %d | Length %d | Linmem %f | After life usage %d\n", world->loadedChunks.capacity, world->loadedChunks.length,
	       (float)linearSpaceFree() / (1024.f * 1024.f), afterLifeUsage);
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
	for (i = 0; i < CHUNK_AFTERLIFE_SIZE; i++)
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

	ChunkWorker_AddJob(cworker, chunk, ChunkWorker_TaskOpenChunk);

	return chunk;
}

void World_FreeChunk(World* world, Chunk* chunk) {
	for (int j = 0; j < CHUNK_CLUSTER_COUNT; j++) {
		VBO_Free(chunk->data[j].vbo[0]);
		VBO_Free(chunk->data[j].vbo[1]);
	}
	poolFree(&world->chunkpool, chunk);
}

void World_ReleaseChunk(World* world, int x, int z) {
	Chunk* chunk;
	int i;
	vec_foreach (&world->loadedChunks, chunk, i)
		if (chunk->x == x && chunk->z == z)
			if (--chunk->referenced == 0) {
				vec_splice(&world->loadedChunks, i, 1);

				for (int j = 0; j < CHUNK_AFTERLIFE_SIZE; j++) {
					if (world->afterLife[j] == NULL) {
						world->afterLife[j] = chunk;

						ChunkWorker_AddJob(cworker, chunk, ChunkWorker_TaskSaveChunk);
						break;
					}
					if (j == CHUNK_AFTERLIFE_SIZE - 1) {
						World_FreeChunk(world, chunk);
					}
				}

				return;
			}
}

void World_ClearAfterLife(World* world) {
	for (int i = 0; i < CHUNK_AFTERLIFE_SIZE; i++) {
		if (world->afterLife[i] != NULL) {
			if (world->afterLife[i]->referenced < 0) {  // -2 ist richtig
				if (!world->afterLife[i]->tasksPending) {
					World_FreeChunk(world, world->afterLife[i]);
					world->afterLife[i] = NULL;
					// printf("Freed chunk\n");
				}
				// printf("Freed chunk in after life\n");
			} else {
				world->afterLife[i]->referenced--;
				// printf("Decreased chunk counter in afterlife %d\n", world->afterLife[i]->referenced);
			}
		}
	}
}

void World_Tick(World* world) {
	RecursiveLock_Lock(&worldLock);

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
							for (int j = 0; j < CACHE_SIZE; j++) {
								CACHE_AXIS = cache->cache[!axis ? (i + delta) : j][!axis ? j : (i + delta)];
							}
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

	for (int x = 0; x < CACHE_SIZE; x++) {
		for (int z = 0; z < CACHE_SIZE; z++) {
			Chunk* chunk = cache->cache[x][z];
			if (chunk->worldGenProgress == WorldGenProgress_Empty && !chunk->tasksPending) {
				ChunkWorker_AddJob(cworker, chunk, ChunkWorker_TaskBaseGeneration);
			}

			if (x > 0 && z > 0 && x < CACHE_SIZE - 1 && z < CACHE_SIZE - 1 && chunk->worldGenProgress == WorldGenProgress_Terrain) {
				bool stillTasksToDo = false;
				for (int xO = -1; xO < 2; xO++)
					for (int zO = -1; zO < 2; zO++) stillTasksToDo |= cache->cache[x + xO][z + zO]->tasksPending;

				if (!stillTasksToDo) {
					ChunkWorker_AddJob(cworker, chunk, ChunkWorker_TaskDecorateChunk);
					// ChunkWorker_AddJob(cworker, chunk, ChunkWorker_TaskPolygonizeChunk);
				}
			}

			for (int y = 0; y < CHUNK_CLUSTER_COUNT; y++) {
				Cluster* cluster = &chunk->data[y];
				for (int i = 0; i < 3; i++) {
					uint32_t rX = randN(&world->tickRandom, CHUNK_WIDTH);
					uint32_t rY = randN(&world->tickRandom, CHUNK_CLUSTER_HEIGHT);
					uint32_t rZ = randN(&world->tickRandom, CHUNK_DEPTH);

					// printf("Ticking %d %d %d", rX, rY, rZ);

					Blocks_RandomTick((void*)world, cache->where.startX * CHUNK_WIDTH + x * CHUNK_WIDTH + rX, y * CHUNK_CLUSTER_HEIGHT + rY,
							  cache->where.startY * CHUNK_DEPTH + z * CHUNK_DEPTH + rZ);
				}
			}
		}
	}

	RecursiveLock_Unlock(&worldLock);
}

void World_SetBlock(World* world, int x, int y, int z, Block block) {
	RecursiveLock_Lock(&worldLock);
	ChunkCache* cache = world->cache[0];
	world->errFlags = 0;
	if (Box_IsPointInside(cache->where, BlockToChunkCoordZ(x), BlockToChunkCoordZ(z)) && y >= 0 && y < CHUNK_HEIGHT) {
		int chunkX = ChunkCache_LocalizeChunkX(cache, x), chunkZ = ChunkCache_LocalizeChunkZ(cache, z);
		int lX = Chunk_GetLocalX(x), lZ = Chunk_GetLocalZ(z);

		if (cache->cache[chunkX][chunkZ]->tasksPending > 0) {
			world->errFlags |= World_ErrLockedBlockRequested;
			RecursiveLock_Unlock(&worldLock);
			return;
		}
		if (Chunk_GetBlock(cache->cache[chunkX][chunkZ], lX, y, lZ) != block)
			Chunk_SetBlock(cache->cache[chunkX][chunkZ], lX, y, lZ, block);
		else {
			RecursiveLock_Unlock(&worldLock);
			return;
		}

		int clusterY = y / CHUNK_CLUSTER_HEIGHT;
		if (block != Block_Air) cache->cache[chunkX][chunkZ]->data[clusterY].flags &= ~ClusterFlags_Empty;

		if (lX == 0 && chunkX - 1 >= 0) Chunk_MarkCluster(cache->cache[chunkX - 1][chunkZ], clusterY, ClusterFlags_VBODirty);
		if (lX == CHUNK_WIDTH - 1 && chunkX + 1 < CACHE_SIZE) Chunk_MarkCluster(cache->cache[chunkX + 1][chunkZ], clusterY, ClusterFlags_VBODirty);
		if (lZ == 0 && chunkZ - 1 >= 0) Chunk_MarkCluster(cache->cache[chunkX][chunkZ - 1], clusterY, ClusterFlags_VBODirty);
		if (lZ == CHUNK_DEPTH - 1 && chunkZ + 1 < CACHE_SIZE) Chunk_MarkCluster(cache->cache[chunkX][chunkZ + 1], clusterY, ClusterFlags_VBODirty);
		int lY = y - (y / CHUNK_CLUSTER_HEIGHT * CHUNK_CLUSTER_HEIGHT);
		if (lY == 0 && clusterY - 1 >= 0) Chunk_MarkCluster(cache->cache[chunkX][chunkZ], clusterY - 1, ClusterFlags_VBODirty);
		if (lY == CHUNK_CLUSTER_HEIGHT - 1 && clusterY < CHUNK_CLUSTER_HEIGHT) Chunk_MarkCluster(cache->cache[chunkX][chunkZ], clusterY + 1, ClusterFlags_VBODirty);

		RecursiveLock_Unlock(&worldLock);
		return;
	}
	world->errFlags |= World_ErrUnloadedBlockRequested;
	RecursiveLock_Unlock(&worldLock);
}

Block World_GetBlock(World* world, int x, int y, int z) {
	RecursiveLock_Lock(&worldLock);
	ChunkCache* cache = world->cache[0];
	world->errFlags = 0;
	if (Box_IsPointInside(cache->where, BlockToChunkCoordZ(x), BlockToChunkCoordZ(z)) && y >= 0 && y < CHUNK_HEIGHT) {
		int chunkX = ChunkCache_LocalizeChunkX(cache, x), chunkZ = ChunkCache_LocalizeChunkZ(cache, z);

		RecursiveLock_Unlock(&worldLock);
		return Chunk_GetBlock(cache->cache[chunkX][chunkZ], Chunk_GetLocalX(x), y, Chunk_GetLocalZ(z));
	}
	world->errFlags |= World_ErrUnloadedBlockRequested;
	RecursiveLock_Unlock(&worldLock);
	return Block_Air;
}

void Chunk_RecalcHeightMap(Chunk* chunk) {
	if (chunk->heightMapEdit != chunk->editsCounter) {
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
		chunk->heightMapEdit = chunk->editsCounter;
	}
}

Chunk* World_FastChunkAccess(World* world, int x, int z) {
	ChunkCache* cache = world->cache[0];
	world->errFlags = 0;
	if (Box_IsPointInside(cache->where, BlockToChunkCoordZ(x), BlockToChunkCoordZ(z))) {
		int chunkX = ChunkCache_LocalizeChunkX(cache, x), chunkZ = ChunkCache_LocalizeChunkZ(cache, z);

		return cache->cache[chunkX][chunkZ];
	}
	world->errFlags |= World_ErrUnloadedBlockRequested;
	return NULL;
}

int World_GetHeight(World* world, int x, int z) {
	ChunkCache* cache = world->cache[0];
	world->errFlags = 0;
	if (Box_IsPointInside(cache->where, BlockToChunkCoordZ(x), BlockToChunkCoordZ(z))) {
		int chunkX = ChunkCache_LocalizeChunkX(cache, x), chunkZ = ChunkCache_LocalizeChunkZ(cache, z);

		return cache->cache[chunkX][chunkZ]->heightmap[Chunk_GetLocalX(x)][Chunk_GetLocalZ(z)];
	}
	world->errFlags |= World_ErrUnloadedBlockRequested;
	return 0;
}