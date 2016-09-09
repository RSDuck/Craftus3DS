#ifndef CHUNKWORKER_H_INCLUDED
#define CHUNKWORKER_H_INCLUDED

#include <3ds.h>

#include <vec/vec.h>

#include "world.h"

#define CHUNKWORKER_THREAD_STACKSIZE (8 * 1024)

typedef enum {
	ChunkWorker_TaskOpenChunk,
	ChunkWorker_TaskSaveChunk,
	ChunkWorker_TaskDecorateChunk,
	ChunkWorker_TaskPolygonizeChunk,
	ChunkWorker_TaskTypeCount
} ChunkWorker_TaskType;
typedef struct {
	ChunkWorker_TaskType type;
	World* world;
	Chunk* chunk;
} ChunkWorker_Task;

typedef vec_t(ChunkWorker_Task) ChunkWorker_Queue;

typedef bool (*ChunkWorker_HandlerFunc)(ChunkWorker_Queue*, ChunkWorker_Task);
typedef struct {
	ChunkWorker_HandlerFunc func;
	int priority;
} ChunkWorker_Handler;

typedef struct {
	World* world;

	Thread thread;
	LightLock lock;

	ChunkWorker_Queue queue[2];
	int currentQueue;

	vec_t(ChunkWorker_Handler) handler[ChunkWorker_TaskTypeCount];
} ChunkWorker;

// Dürfen nur aus dem Mainthread aufgerufen werden!!
ChunkWorker* ChunkWorker_New(World* world);
void ChunkWorker_Stop(ChunkWorker* worker);

void ChunkWorker_AddHandler(ChunkWorker* worker, ChunkWorker_TaskType type, ChunkWorker_HandlerFunc handlerFunc, int priority);
void ChunkWorker_AddJob(ChunkWorker* worker, Chunk* chunk, ChunkWorker_TaskType type);

void ChunkWorker_Main(void* args);

#endif  // !CHUNKWORKER_H_INCLUDED