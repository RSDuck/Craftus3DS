#include "craftus/world/blocks.h"

void Blocks_GetUVs(Block block, Direction side, float* out) {
	const float texOffset = 1.f / 4.f;
	switch (block) {
		case Block_Air:
			break;
		case Block_Dirt:
			out[0] = 0.f;
			out[1] = 0.f;
			break;
		case Block_Stone:
			out[0] = 2.f;
			out[1] = 3.f;
			break;
		case Block_Grass:
			switch (side) {
				case Direction_Top:
					out[0] = 0.f;
					out[1] = 2.f;
					break;
				case Direction_Bottom:
					out[0] = 0.f;
					out[1] = 0.f;
					break;
				default:
					out[0] = 0.f;
					out[1] = 1.f;
					break;
			}
			break;
	}
	out[0] *= texOffset;
	out[1] *= texOffset;
}