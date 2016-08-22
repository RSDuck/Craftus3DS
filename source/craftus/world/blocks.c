#include "craftus/world/blocks.h"

#define PREFIX "romfs:/textures/blocks/"

#define STONEP PREFIX "stone.png"
#define GRASSTOPP PREFIX "grass_top.png"
#define GRASSSIDEP PREFIX "grass_side.png"
#define DIRTP PREFIX "dirt.png"
#define COBBLEP PREFIX "cobblestone.png"
#define SANDP PREFIX "sand.png"
#define STONEBRICKP PREFIX "stonebrick.png"

static Texture_MapIcon grassTopTex;
static Texture_MapIcon grassSideTex;
static Texture_MapIcon stoneTex;
static Texture_MapIcon dirtTex;
static Texture_MapIcon cobbleTex;
static Texture_MapIcon sandTex;
static Texture_MapIcon stonebrickTex;

void Blocks_InitTexture(Texture_Map* map) {
	Texture_MapInit(map, STONEP ";" GRASSTOPP ";" GRASSSIDEP ";" DIRTP ";" COBBLEP ";" SANDP ";" STONEBRICKP);
	grassTopTex = Texture_MapGetIcon(map, GRASSTOPP);
	grassSideTex = Texture_MapGetIcon(map, GRASSSIDEP);
	stoneTex = Texture_MapGetIcon(map, STONEP);
	dirtTex = Texture_MapGetIcon(map, DIRTP);
	cobbleTex = Texture_MapGetIcon(map, COBBLEP);
	sandTex = Texture_MapGetIcon(map, SANDP);
	stonebrickTex = Texture_MapGetIcon(map, STONEBRICKP);
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
		case Block_Cobblestone:
			out[0] = cobbleTex.u;
			out[1] = cobbleTex.v;
			break;
		case Block_Sand:
			out[0] = sandTex.u;
			out[1] = sandTex.v;
			break;
		case Block_StoneBrick:
			out[0] = stonebrickTex.u;
			out[1] = stonebrickTex.v;
			break;
	}
}

const char* Blocks_GetNameStr(Block block) {
	switch (block) {
		case Block_Air:
			return "Air";
		case Block_Stone:
			return "Stone";
		case Block_Dirt:
			return "Dirt";
		case Block_Grass:
			return "Grass";
		case Block_Cobblestone:
			return "Cobblestone";
		case Block_Sand:
			return "Sand";
		case Block_StoneBrick:
			return "Stonebrick";
		case Blocks_Count:
			return "Report an error if you see this text";
	}
}