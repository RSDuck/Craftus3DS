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
	worker->thread = threadCreate(&ChunkWorker_Main, (void*)worker, CHUNKWORKER_THREAD_STACKSIZE, 0x18, 1, false);
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

static int priority_sort(const void* a, const void* b) { return ((ChunkWorker_Handler*)b)->priority - ((ChunkWorker_Handler*)a)->priority; }

void ChunkWorker_AddHandler(ChunkWorker* worker, ChunkWorker_TaskType type, ChunkWorker_HandlerFunc handlerFunc, int priority) {
	LightLock_Lock(&worker->lock);

	ChunkWorker_Handler handler;
	handler.func = handlerFunc;
	handler.priority = priority;
	vec_push(&worker->handler[type], handler);

	vec_sort(&worker->handler[type], &priority_sort);

	LightLock_Unlock(&worker->lock);
}

void ChunkWorker_AddJob(ChunkWorker* worker, Chunk* chunk, ChunkWorker_TaskType type) {
	chunk->flags |= ClusterFlags_InProcess;

	ChunkWorker_Task task;
	task.type = type;
	task.chunk = chunk;
	vec_push(&worker->queue[worker->currentQueue], task);
}

void ChunkWorker_Main(void* args) {
	printf("Hello from a worker thread\n");
	ChunkWorker* worker = (ChunkWorker*)args;
	vec_t(Chunk*)unlockList;
	vec_init(&unlockList);
	while (worker != workerToStop) {
		if (!LightLock_TryLock(&worker->lock)) {
			int operatingOn = worker->currentQueue;
			worker->currentQueue ^= 1;

			while (worker->queue[operatingOn].length > 0) {
				ChunkWorker_Task task = vec_pop(&worker->queue[operatingOn]);
				// printf("Handling task of type %d\n", task.type);

				for (int i = 0; i < worker->handler[task.type].length; i++) {
					worker->handler[task.type].data[i].func(&worker->queue[operatingOn], task);
				}
				vec_push(&unlockList, task.chunk);

				svcSleepThread(100);
			}

			while (unlockList.length > 0) vec_pop(&unlockList)->flags &= ~ClusterFlags_InProcess;

			LightLock_Unlock(&worker->lock);

			svcSleepThread(100000);
		}
	}
	vec_deinit(&unlockList);
	printf("It's time so say goodbye for the worker thread\n");
}