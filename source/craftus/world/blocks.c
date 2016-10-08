#include "craftus/world/blocks.h"

#include "craftus/world/world.h"

#define PREFIX "romfs:/textures/blocks/"

#define STONEP PREFIX "stone.png"
#define GRASSTOPP PREFIX "grass_top.png"
#define GRASSSIDEP PREFIX "grass_side.png"
#define DIRTP PREFIX "dirt.png"
#define COBBLEP PREFIX "cobblestone.png"
#define SANDP PREFIX "sand.png"
#define STONEBRICKP PREFIX "stonebrick.png"
#define LOGSIDEP PREFIX "log_oak.png"
#define LOGTOPP PREFIX "log_oak_top.png"
#define LEAVESP PREFIX "leaves_oak1.png"
#define PLANKSP PREFIX "planks_oak.png"
#define GLASSP PREFIX "glass.png"

static Texture_MapIcon grassTopTex;
static Texture_MapIcon grassSideTex;
static Texture_MapIcon stoneTex;
static Texture_MapIcon dirtTex;
static Texture_MapIcon cobbleTex;
static Texture_MapIcon sandTex;
static Texture_MapIcon stonebrickTex;
static Texture_MapIcon logSideTex;
static Texture_MapIcon logTopTex;
static Texture_MapIcon leavesOakTex;
static Texture_MapIcon planksTex;
static Texture_MapIcon glassTex;

void Blocks_InitTexture(Texture_Map* map) {
	Texture_MapInit(map, STONEP ";" GRASSTOPP ";" GRASSSIDEP ";" DIRTP ";" COBBLEP ";" SANDP ";" STONEBRICKP ";" LOGSIDEP ";" LOGTOPP ";" LEAVESP ";" PLANKSP ";" GLASSP);
	grassTopTex = Texture_MapGetIcon(map, GRASSTOPP);
	grassSideTex = Texture_MapGetIcon(map, GRASSSIDEP);
	stoneTex = Texture_MapGetIcon(map, STONEP);
	dirtTex = Texture_MapGetIcon(map, DIRTP);
	cobbleTex = Texture_MapGetIcon(map, COBBLEP);
	sandTex = Texture_MapGetIcon(map, SANDP);
	stonebrickTex = Texture_MapGetIcon(map, STONEBRICKP);
	logSideTex = Texture_MapGetIcon(map, LOGSIDEP);
	logTopTex = Texture_MapGetIcon(map, LOGTOPP);
	leavesOakTex = Texture_MapGetIcon(map, LEAVESP);
	planksTex = Texture_MapGetIcon(map, PLANKSP);
	glassTex = Texture_MapGetIcon(map, GLASSP);
}

Block_Shape Blocks_GetShape(Block block) { return BlockShape_Cube; }

bool Blocks_IsOpaque(Block block) {
	switch (block) {
		case Block_Glass:
			return false;
		case Block_Leaves:
			return false;
		default:
			return true;
	}
}

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
		case Block_Log:
			switch (side) {
				case Direction_Top:
					out[0] = logTopTex.u;
					out[1] = logTopTex.v;
					break;
				default:
					out[0] = logSideTex.u;
					out[1] = logSideTex.v;
					break;
			}
			break;
		case Block_Glass:
			out[0] = glassTex.u;
			out[1] = glassTex.v;
			break;
		case Block_Leaves:
			out[0] = leavesOakTex.u;
			out[1] = leavesOakTex.v;
			break;
		case Block_Planks:
			out[0] = planksTex.u;
			out[1] = planksTex.v;
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
		case Block_Glass:
			return "Glass";
		case Block_Leaves:
			return "Oak Leaves";
		case Block_Log:
			return "Oak Log";
		case Block_Planks:
			return "Oak Planks";
		default:
			return "Report an error if you see this text";
	}
}

void Blocks_RandomTick(void* w, int x, int y, int z) {
	World* world = (World*)w;
	switch (World_GetBlock(world, x, y, z)) {
		case Block_Dirt:
			if (World_GetBlock(world, x, y + 1, z) == Block_Air) {
				World_SetBlock(world, x, y, z, Block_Grass);
			}
			break;
		case Block_Grass:
			if (World_GetBlock(world, x, y + 1, z) != Block_Air) {
				World_SetBlock(world, x, y, z, Block_Dirt);
			}
			break;
	}
}