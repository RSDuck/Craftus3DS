#include "craftus/world/blocks.h"

void Blocks_GetUVs(Block block, Direction side, uint8_t* out) {
	switch (block) {
		case Block_Air:
			break;
		case Block_Dirt:
			out[0] = 0;
			out[1] = 0;
			break;
		case Block_Stone:
			out[0] = 2;
			out[1] = 3;
			break;
		case Block_Grass:
			switch (side) {
				case Direction_Top:
					out[0] = 0;
					out[1] = 2;
					break;
				case Direction_Bottom:
					out[0] = 0;
					out[1] = 0;
					break;
				default:
					out[0] = 0;
					out[1] = 1;
					break;
			}
			break;
	}
	out[0] = (((out[0] * 16) + (out[0] * 2)) * 2);
	out[1] = (((out[1] * 16) + (out[1] * 2)) * 2);
}