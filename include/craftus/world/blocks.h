#ifndef BLOCK_H_INCLUDED
#define BLOCK_H_INCLUDED

#include "craftus/world/direction.h"

typedef enum { Block_Air = 0, Block_Stone, Block_Dirt, Block_Grass } Block;

void Blocks_GetUVs(Block block, Direction side, float* out);

#endif  // !BLOCK_H_INCLUDED 