#include "craftus/world/savemanager.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <cNBT/nbt.h>

static World* workingWorld;

static void saveManifest() {
	char tmpStr[100];

	mkdir(workingWorld->name);

	sprintf(tmpStr, "./craftus/saves/%s/level.dat", workingWorld->name);
}

static void readManifest() {
	char tmpStr[100];
	sprintf(tmpStr, "./craftus/saves/%s/level.dat", workingWorld->name);
}

void SaveManager_Init(World* world) {
	workingWorld = world;

	char tmpStr[100];

	sprintf(tmpStr, "./craftus/saves/%s", workingWorld->name);
	DIR* worldDir = opendir(tmpStr);
	if (errno == ENOENT) {
		saveManifest();
	} else {
		readManifest();
	}
}

void SaveManager_Free() { saveManifest(); }

void SaveManager_Flush() {}

bool SaveManager_SaveChunk(ChunkWorker_Queue* queue, ChunkWorker_Task task) {}

bool SaveManager_LoadChunk(ChunkWorker_Queue* queue, ChunkWorker_Task task) {}
