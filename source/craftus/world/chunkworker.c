#include "craftus/world/chunkworker.h"

#include <stdio.h>

ChunkWorker* ChunkWorker_New(World* world) {
	ChunkWorker* worker = (ChunkWorker*)malloc(sizeof(ChunkWorker));

	worker->world = world;
	worker->currentQueue = 0;

	LightLock_Init(&worker->lock);

	vec_init(&worker->queue[0]);
	vec_init(&worker->queue[1]);

	for (int i = 0; i < ChunkWorker_TaskTypeCount; i++) vec_init(&worker->handler[i]);

	int prio;
	svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
	worker->thread = threadCreate(&ChunkWorker_Main, (void*)worker, CHUNKWORKER_THREAD_STACKSIZE, prio - 1, 1, false);
	if (!worker->thread) {
		printf("Failed to create chunk worker thread\n");
	}

	return worker;
}

volatile ChunkWorker* workerToStop = NULL;  // Nicht Thread sicher aber egal
void ChunkWorker_Stop(ChunkWorker* worker) {
	// Thread zum Ende bringen
	workerToStop = worker;
	threadJoin(worker->thread, U64_MAX);
	threadFree(worker->thread);

	vec_deinit(&worker->queue[0]);
	vec_deinit(&worker->queue[1]);

	for (int i = 0; i < ChunkWorker_TaskTypeCount; i++) vec_deinit(&worker->handler[i]);

	free(worker);
}

static int priority_sort(const void* a, const void* b) { return ((ChunkWorker_Handler*)a)->priority - ((ChunkWorker_Handler*)b)->priority; }

void ChunkWorker_AddHandler(ChunkWorker* worker, ChunkWorker_TaskType type, ChunkWorker_HandlerFunc handlerFunc, int priority) {
	LightLock_Lock(&worker->lock);

	ChunkWorker_Handler handler;
	handler.func = handlerFunc;
	handler.priority = priority;
	vec_push(&worker->handler[type], handler);

	// vec_sort(&worker->handler[type], &priority_sort);

	LightLock_Unlock(&worker->lock);
}

void ChunkWorker_AddJob(ChunkWorker* worker, Chunk* chunk, ChunkWorker_TaskType type) {
	chunk->flags |= ClusterFlags_InProcess;

	ChunkWorker_Task task;
	task.type = type;
	task.chunk = chunk;
	task.world = worker->world;
	vec_push(&worker->queue[worker->currentQueue], task);
}

void ChunkWorker_Main(void* args) {
	// printf("Hello from a worker thread\n");
	ChunkWorker* worker = (ChunkWorker*)args;
	while (worker != workerToStop) {
		if (!LightLock_TryLock(&worker->lock)) {
			int operatingOn = worker->currentQueue;
			worker->currentQueue ^= 1;

			while (worker->queue[operatingOn].length > 0) {
				ChunkWorker_Task task = vec_pop(&worker->queue[operatingOn]);

				for (int i = 0; i < worker->handler[task.type].length; i++) {
					worker->handler[task.type].data[i].func(&worker->queue[operatingOn], task);
				}
				// vec_push(&unlockList, task.chunk);
				task.chunk->flags &= ~ClusterFlags_InProcess;

				svcSleepThread(1000110);
			}

			LightLock_Unlock(&worker->lock);

			svcSleepThread(1000000);
		}
	}
	printf("It's time so say goodbye for the worker thread\n");
}