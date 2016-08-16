#ifndef TEXTUREMAP_H_INCLUDED
#define TEXTUREMAP_H_INCLUDED

#include <c3d/texture.h>
#include <stdint.h>

void Texture_Load(C3D_Tex* result, char* filename);

#define TEXTURE_MAPSIZE 128
#define TEXTURE_TILESIZE 16
#define TEXTURE_MAPTILES (TEXTURE_MAPSIZE / TEXTURE_TILESIZE)

typedef struct {
	uint32_t textureHash;
	uint8_t u, v;
} Texture_MapIcon;

typedef struct {
	C3D_Tex texture;
	Texture_MapIcon icons[TEXTURE_MAPTILES * TEXTURE_MAPTILES];
} Texture_Map;

void Texture_MapInit(Texture_Map* map, const char* files);
Texture_MapIcon Texture_MapGetIcon(Texture_Map* map, const char* filename);

#endif  // !TEXTUREMAP_H_INCLUDED