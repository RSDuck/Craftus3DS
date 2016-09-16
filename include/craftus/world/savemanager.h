#ifndef SAVEMANAGER_H_INCLUDED

#include "craftus/entity/player.h"
#include "craftus/world/chunkworker.h"
#include "craftus/world/world.h"

void SaveManager_Init(World* world, Player* player);
void SaveManager_Free();

void SaveManager_Flush();

bool SaveManager_SaveChunk(ChunkWorker_Queue* queue, ChunkWorker_Task task);
bool SaveManager_LoadChunk(ChunkWorker_Queue* queue, ChunkWorker_Task task);

#endif  // !SAVEMANAGER_H_INCLUDED