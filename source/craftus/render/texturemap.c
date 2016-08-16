#include "craftus/render/texturemap.h"

#include "lodepng/lodepng.h"

#include <3ds.h>
#include <citro3d.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>

uint32_t hash(char* str) {
	unsigned long hash = 5381;
	int c;
	while ((c = *str++)) hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	return hash;
}

void Texture_Load(C3D_Tex* result, char* filename) {}

static void downscaleImage(u8* orgin, u8* target, int width, int height) {
	int i, j;
	for (j = 0; j < height; j++) {
		for (i = 0; i < width; i++) {
			const u32 offset = (i + j * width) * 4;
			const u32 offset2 = (i * 2 + j * 2 * width * 2) * 4;
			target[offset + 0] = (orgin[offset2 + 0 + 0] + orgin[offset2 + 4 + 0] + orgin[offset2 + width * 4 * 2 + 0] + orgin[offset2 + (width * 2 + 1) * 4 + 0]) / 4;
			target[offset + 1] = (orgin[offset2 + 0 + 1] + orgin[offset2 + 4 + 1] + orgin[offset2 + width * 4 * 2 + 1] + orgin[offset2 + (width * 2 + 1) * 4 + 1]) / 4;
			target[offset + 2] = (orgin[offset2 + 0 + 2] + orgin[offset2 + 4 + 2] + orgin[offset2 + width * 4 * 2 + 2] + orgin[offset2 + (width * 2 + 1) * 4 + 2]) / 4;
			target[offset + 3] = (orgin[offset2 + 0 + 3] + orgin[offset2 + 4 + 3] + orgin[offset2 + width * 4 * 2 + 3] + orgin[offset2 + (width * 2 + 1) * 4 + 3]) / 4;
		}
	}
}

u8 tileOrder[] = {0,  1,  8,  9,  2,  3,  10, 11, 16, 17, 24, 25, 18, 19, 26, 27, 4,  5,  12, 13, 6,  7,  14, 15, 20, 21, 28, 29, 22, 23, 30, 31,
		  32, 33, 40, 41, 34, 35, 42, 43, 48, 49, 56, 57, 50, 51, 58, 59, 36, 37, 44, 45, 38, 39, 46, 47, 52, 53, 60, 61, 54, 55, 62, 63};
void tileImage32(u32* src, u32* dst, int width, int height) {
	if (!src || !dst) return;

	int i, j, k, l;
	l = 0;
	for (j = 0; j < height; j += 8) {
		for (i = 0; i < width; i += 8) {
			for (k = 0; k < 8 * 8; k++) {
				int x = i + tileOrder[k] % 8;
				int y = j + (tileOrder[k] - (x - i)) / 8;
				u32 v = src[x + (height - 1 - y) * width];
				dst[l++] = __builtin_bswap32(v);
			}
		}
	}
}

void Texture_MapInit(Texture_Map* map, char* files) {
	int locX = 0;
	int locY = 0;

	char modableFiles[512];
	strcpy(modableFiles, files);

	printf("TextureMapInit %s\n", files);

	const int mipmapLevels = 7;
	const int maxSize = 4 * TEXTURE_MAPSIZE * TEXTURE_MAPSIZE;
	const int mipmappedSize = ((maxSize - (maxSize >> (2 * (mipmapLevels + 1)))) * 4) / 3;

	uint32_t* buffer = (uint32_t*)linearAlloc(mipmappedSize);

	char* filename = strtok(modableFiles, ";");
	int c = 0;
	while (filename != NULL && c < (TEXTURE_MAPTILES * TEXTURE_MAPTILES)) {
		uint32_t *image, w, h;
		uint32_t error = lodepng_decode32_file((uint8_t**)&image, &w, &h, filename);
		if (w == TEXTURE_TILESIZE && h == TEXTURE_TILESIZE && image != NULL && !error) {
			for (int x = 0; x < TEXTURE_TILESIZE; x++) {
				for (int y = 0; y < TEXTURE_TILESIZE; y++) {
					buffer[(locX + x) + ((y + locY) * TEXTURE_MAPSIZE)] = __builtin_bswap32(image[x + ((TEXTURE_TILESIZE - y - 1) * TEXTURE_TILESIZE)]);
				}
			}

			Texture_MapIcon* icon = &map->icons[c];
			icon->textureHash = hash(filename);
			icon->u = 2 * locX;
			icon->v = 2 * locY;

			printf("Stiched texture %s(hash: %u) at %d, %d\n", filename, icon->textureHash, locX, locY);

			locX += TEXTURE_TILESIZE;
			if (locX == TEXTURE_MAPSIZE) locY += TEXTURE_TILESIZE;
		} else {
			printf("Image size(%d, %d) doesn't match or ptr null\n'", w, h);
		}
		free(image);
		filename = strtok(NULL, ";");
		c++;
	}

	uint32_t offset = TEXTURE_MAPSIZE * TEXTURE_MAPSIZE;
	uint32_t prevLvlOff = 0;
	int size = TEXTURE_MAPSIZE / 2;
	uint32_t* tmpBuf = (uint32_t*)malloc(size);
	for (int i = 0; i < mipmapLevels; i++) {
		downscaleImage((u8*)(buffer + prevLvlOff), (u8*)tmpBuf, size, size);
		tileImage32(tmpBuf, buffer + offset, size, size);
		prevLvlOff = offset;
		offset += size * size;
		size /= 2;
	}
	free(tmpBuf);

	GSPGPU_FlushDataCache(buffer, 4 * TEXTURE_MAPSIZE * TEXTURE_MAPSIZE);

	C3D_TexInitVRAM(&map->texture, TEXTURE_MAPSIZE, TEXTURE_MAPSIZE, GPU_RGBA8);
	C3D_TexSetFilter(&map->texture, GPU_NEAREST, GPU_NEAREST);

	GX_RequestDma(buffer, map->texture.data, mipmappedSize);
	gspWaitForDMA();

	linearFree(buffer);
}

Texture_MapIcon Texture_MapGetIcon(Texture_Map* map, char* filename) {
	uint32_t h = hash(filename);
	for (size_t i = 0; i < TEXTURE_MAPTILES * TEXTURE_MAPTILES; i++) {
		if (h == map->icons[i].textureHash) {
			return map->icons[i];
		}
	}
	return (Texture_MapIcon){0};
}