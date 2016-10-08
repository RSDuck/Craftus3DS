#ifndef BLOCK_H_INCLUDED
#define BLOCK_H_INCLUDED

#include "craftus/render/texturemap.h"
#include "craftus/world/direction.h"

#include <stdint.h>

typedef enum {
	Block_Air = 0,
	Block_Stone,
	Block_Dirt,
	Block_Grass,
	Block_Cobblestone,
	Block_Sand,
	Block_StoneBrick,
	Block_Log,
	Block_Leaves,
	Block_Planks,
	Block_Glass,
	Blocks_Count
} Block;

typedef enum { BlockShape_Cube, BlockShape_Plant } Block_Shape;

void Blocks_InitTexture(Texture_Map* map);
void Blocks_GetUVs(Block block, Direction side, uint8_t* out);
Block_Shape Blocks_GetShape(Block block);
bool Blocks_IsOpaque(Block block);

const char* Blocks_GetNameStr(Block block);

/*#ifndef WORLD_H_INCLUDED
typedef void World;
#endif*/

// TODO: in eine andere Datei aufspalten um void* zu vermeiden
//       --------------+++++
//                     |||||
//                     vvvvv
void Blocks_RandomTick(void* w, int x, int y, int z);
//                     ~~~~

#endif  // !BLOCK_H_INCLUDED