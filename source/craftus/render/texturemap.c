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

void Texture_MapInit(Texture_Map* map, const char* files) {
	int locX = 0;
	int locY = 0;

	char modableFiles[512];
	strcpy(modableFiles, files);

	printf("TextureMapInit %s\n", files);

	uint32_t* buffer = (uint32_t*)linearAlloc(4 * TEXTURE_MAPSIZE * TEXTURE_MAPSIZE);
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
	GSPGPU_FlushDataCache(buffer, 4 * TEXTURE_MAPSIZE * TEXTURE_MAPSIZE);

	C3D_TexInitVRAM(&map->texture, TEXTURE_MAPSIZE, TEXTURE_MAPSIZE, GPU_RGBA8);
	C3D_TexSetFilter(&map->texture, GPU_NEAREST, GPU_NEAREST);

	printf("GX_DisplayTransfer trap...\n");
	C3D_SafeDisplayTransfer(buffer, GX_BUFFER_DIM(TEXTURE_MAPSIZE, TEXTURE_MAPSIZE), map->texture.data, GX_BUFFER_DIM(TEXTURE_MAPSIZE, TEXTURE_MAPSIZE),
				(GX_TRANSFER_FLIP_VERT(1) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) | GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) |
				 GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO)));
	printf("Passed!\n");

	linearFree(buffer);
}

Texture_MapIcon Texture_MapGetIcon(Texture_Map* map, const char* filename) {
	uint32_t h = hash(filename);
	for (size_t i = 0; i < TEXTURE_MAPTILES * TEXTURE_MAPTILES; i++) {
		if (h == map->icons[i].textureHash) {
			return map->icons[i];
		}
	}
	return (Texture_MapIcon){0};
}