#include "craftus/render/vbocache.h"

#include "vec/vec.h"

#include <3ds.h>

static vec_t(VBO_Block) freedBlocks;

void VBOCache_Init() { vec_init(&freedBlocks); }
void VBOCache_Free() {
	VBO_Block block;
	int i;
	vec_foreach (&freedBlocks, block, i) { linearFree(block.memory); }
	vec_deinit(&freedBlocks);
}

VBO_Block VBO_Alloc(size_t size) {
	if (freedBlocks.length > 0) {
		VBO_Block block;
		int i;
		vec_foreach (&freedBlocks, block, i) {
			if (size <= block.size && block.size - size <= 2048) {
				printf("Recycling vbo!\n");
				vec_splice(&freedBlocks, i, 1);
				return block;
			}
		}
	}
	VBO_Block block;
	block.memory = linearAlloc(size);
	block.size = size;
	return block;
}

static int sort_by_size(const void* a, const void* b) { return ((VBO_Block*)b)->size - ((VBO_Block*)a)->size; }

void VBO_Free(VBO_Block block) {
	vec_push(&freedBlocks, block);
	vec_sort(&freedBlocks, &sort_by_size);
	linearFree(block.memory);
}
