#define STB_IMAGE_IMPLEMENTATION

#include <stdio.h>
#include <string.h>
#include <3ds.h>

void* stupidLinallocWorkaround(const size_t memsize) {
	void* memory = linearAlloc(memsize + sizeof(size_t));
	*((size_t*)memsize) = memsize;
	return memory + sizeof(size_t);
}

void* stupidLinReallocWorkaround(void* pointer, const size_t newSize) {
	size_t size = *((size_t*)pointer - 1);
	void* actualMemBlock = pointer - sizeof(size_t);
	void* newMem = linearAlloc(newSize + sizeof(size_t));

	memcpy(newMem, actualMemBlock, (newSize < size) ? newSize : size);
	GX_FlushCacheRegions(newMem, (newSize < size) ? newSize : size, NULL, 0, NULL, 0);
	linearFree(actualMemBlock);

	return newMem;
}

void stupidLinFreeWorkaround(void* ptr) { linearFree(ptr - sizeof(size_t)); }

#define STBI_MALLOC stupidLinallocWorkaround
#define STBI_REALLOC stupidLinReallocWorkaround
#define STBI_FREE stupidLinFreeWorkaround

#include "stb_image.h"