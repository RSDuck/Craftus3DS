#include "craftus/world/blocks.h"

#define PREFIX "romfs:/textures/"
#define STONEP PREFIX "stone.png"
#define GRASSTOPP PREFIX "grass_top.png"
#define GRASSSIDEP PREFIX "grass_side.png"
#define DIRTP PREFIX "dirt.png"

static Texture_MapIcon grassTopTex;
static Texture_MapIcon grassSideTex;
static Texture_MapIcon stoneTex;
static Texture_MapIcon dirtTex;

void Blocks_InitTexture(Texture_Map* map) {
	Texture_MapInit(map, STONEP ";" GRASSTOPP ";" GRASSSIDEP ";" DIRTP);
	grassTopTex = Texture_MapGetIcon(map, GRASSTOPP);
	grassSideTex = Texture_MapGetIcon(map, GRASSSIDEP);
	stoneTex = Texture_MapGetIcon(map, STONEP);
	dirtTex = Texture_MapGetIcon(map, DIRTP);
}

Block_Shape Blocks_GetShape(Block block) { return BlockShape_Cube; }

void Blocks_GetUVs(Block block, Direction side, uint8_t* out) {
	switch (block) {
		case Block_Air:
			break;
		case Block_Dirt:
			out[0] = dirtTex.u;
			out[1] = dirtTex.v;
			break;
		case Block_Stone:
			out[0] = stoneTex.u;
			out[1] = stoneTex.v;
			break;
		case Block_Grass:
			switch (side) {
				case Direction_Top:
					out[0] = grassTopTex.u;
					out[1] = grassTopTex.v;
					break;
				case Direction_Bottom:
					out[0] = dirtTex.u;
					out[1] = dirtTex.v;
					break;
				default:
					out[0] = grassSideTex.u;
					out[1] = grassSideTex.v;
					break;
			}
			break;
	}
}