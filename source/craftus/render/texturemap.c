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

void Texture_Load(C3D_Tex* result, char* filename) {
	uint32_t* image = NULL;
	int width = 255, height = 255;
	uint32_t error = lodepng_decode32_file((uint8_t*)&image, &width, &height, filename);
	if (error == 0 && image != NULL) {
		uint32_t* imgInLinRam = (u32*)linearAlloc(width * height * 4);
		for (int i = 0; i < width * height; i++) {
			imgInLinRam[i] = __builtin_bswap32(image[i]);
		}
		GSPGPU_FlushDataCache(imgInLinRam, width * height * 4);
		free(image);

		C3D_TexInitVRAM(result, width, height, GPU_RGBA8);

		C3D_SafeDisplayTransfer(imgInLinRam, GX_BUFFER_DIM(width, height), result->data, GX_BUFFER_DIM(width, height),
					(GX_TRANSFER_FLIP_VERT(1) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) | GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) |
					 GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO)));
		gspWaitForPPF();

		linearFree(imgInLinRam);
	} else {
		printf("Failed to load %s\n", filename);
	}
}

// Grabbed from Citra Emulator (citra/src/video_core/utils.h)
static inline u32 morton_interleave(u32 x, u32 y) {
	u32 i = (x & 7) | ((y & 7) << 8);  // ---- -210
	i = (i ^ (i << 2)) & 0x1313;       // ---2 --10
	i = (i ^ (i << 1)) & 0x1515;       // ---2 -1-0
	i = (i | (i >> 7)) & 0x3F;
	return i;
}
// Grabbed from Citra Emulator (citra/src/video_core/utils.h)
static inline u32 get_morton_offset(u32 x, u32 y, u32 bytes_per_pixel) {
	u32 i = morton_interleave(x, y);
	unsigned int offset = (x & ~7) * 8;
	return (i + offset) * bytes_per_pixel;
}
void tileImage32(u32* src, u8* dst, int size) {
	int i, j;
	for (j = 0; j < size; j++) {
		for (i = 0; i < size; i++) {
			u32 coarse_y = j & ~7;
			u32 dst_offset = get_morton_offset(i, j, 4) + coarse_y * size * 4;

			u32 v = src[i + (size - 1 - j) * size];
			*(u32*)(dst + dst_offset) = v;
		}
	}
}
void downscaleImage(u8* data, int size) {
	int i, j;
	for (j = 0; j < size; j++) {
		for (i = 0; i < size; i++) {
			const u32 offset = (i + j * size) * 4;
			const u32 offset2 = (i * 2 + j * 2 * size * 2) * 4;
			data[offset + 0] = (data[offset2 + 0 + 0] + data[offset2 + 4 + 0] + data[offset2 + size * 4 * 2 + 0] + data[offset2 + (size * 2 + 1) * 4 + 0]) / 4;
			data[offset + 1] = (data[offset2 + 0 + 1] + data[offset2 + 4 + 1] + data[offset2 + size * 4 * 2 + 1] + data[offset2 + (size * 2 + 1) * 4 + 1]) / 4;
			data[offset + 2] = (data[offset2 + 0 + 2] + data[offset2 + 4 + 2] + data[offset2 + size * 4 * 2 + 2] + data[offset2 + (size * 2 + 1) * 4 + 2]) / 4;
			data[offset + 3] = (data[offset2 + 0 + 3] + data[offset2 + 4 + 3] + data[offset2 + size * 4 * 2 + 3] + data[offset2 + (size * 2 + 1) * 4 + 3]) / 4;
		}
	}
}

void Texture_MapInit(Texture_Map* map, char* files) {
	int locX = 0;
	int locY = 0;

	char modableFiles[512];
	strcpy(modableFiles, files);

	// printf("TextureMapInit %s\n", files);

	const int mipmapLevels = 2;
	const int maxSize = 4 * TEXTURE_MAPSIZE * TEXTURE_MAPSIZE;

	uint32_t* buffer = (uint32_t*)linearAlloc(maxSize);

	char* filename = strtok(modableFiles, ";");
	int c = 0;
	while (filename != NULL && c < (TEXTURE_MAPTILES * TEXTURE_MAPTILES)) {
		uint32_t *image, w, h;
		uint32_t error = lodepng_decode32_file((uint8_t*)&image, &w, &h, filename);
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

			// printf("Stiched texture %s(hash: %u) at %d, %d\n", filename, icon->textureHash, locX, locY);

			locX += TEXTURE_TILESIZE;
			if (locX == TEXTURE_MAPSIZE) locY += TEXTURE_TILESIZE;
		} else {
			printf("Image size(%d, %d) doesn't match or ptr null(internal error)\n'", w, h);
		}
		free(image);
		filename = strtok(NULL, ";");
		c++;
	}

	GSPGPU_FlushDataCache(buffer, maxSize);

	if (!C3D_TexInitMipMapVRAM(&map->texture, TEXTURE_MAPSIZE, TEXTURE_MAPSIZE, GPU_RGBA8, mipmapLevels)) printf("Couldn't alloc texture memory\n");
	C3D_TexSetFilter(&map->texture, GPU_NEAREST, GPU_NEAREST);

	C3D_SafeDisplayTransfer(buffer, GX_BUFFER_DIM(TEXTURE_MAPSIZE, TEXTURE_MAPSIZE), map->texture.data, GX_BUFFER_DIM(TEXTURE_MAPSIZE, TEXTURE_MAPSIZE),
				(GX_TRANSFER_FLIP_VERT(1) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) | GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) |
				 GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO)));
	gspWaitForPPF();

	int size = TEXTURE_MAPSIZE / 2;
	ptrdiff_t offset = TEXTURE_MAPSIZE * TEXTURE_MAPSIZE;

	u32* tiledImage = linearAlloc(size * size * 4);

	for (int i = 0; i < mipmapLevels; i++) {
		downscaleImage((u8*)buffer, size);

		tileImage32(buffer, (u8*)tiledImage, size);

		GSPGPU_FlushDataCache(tiledImage, size * size * 4);

		GX_RequestDma(tiledImage, ((u32*)map->texture.data) + offset, size * size * 4);
		gspWaitForAnyEvent();

		offset += size * size;
		size /= 2;
	}

	linearFree(tiledImage);
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