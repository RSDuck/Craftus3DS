#ifndef BLOCK_H_INCLUDED
#define BLOCK_H_INCLUDED

#include "craftus/world/direction.h"

#include <stdint.h>

typedef enum { Block_Air = 0, Block_Stone, Block_Dirt, Block_Grass } Block;

typedef enum { BlockShape_Cube, BlockShape_Plant } Block_Shapes;

void Blocks_GetUVs(Block block, Direction side, uint8_t* out);
void Blocks_GetShape();

#endif  // !BLOCK_H_INCLUDED