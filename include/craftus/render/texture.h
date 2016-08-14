#ifndef TEXTURE_H_INCLUDED

#include <citro3d.h>

#include <stdint.h>

void Texture_Load(C3D_Tex* result, char* filename);

#define TEXTURE_MAPSEAM 1
#define TEXTURE_MAPSIZE 256
#define TEXTURE_TILESIZE 16
#define TEXTURE_MAPTILEROW ((TEXTURE_MAPSIZE - (TEXTURE_MAPSIZE / TEXTURE_TILESIZE * TEXTURE_MAPSEAM)) / TEXTURE_TILESIZE)
#define TEXTURE_MAPTILES (TEXTURE_MAPTILEROW * TEXTURE_MAPTILEROW)

typedef struct {
	uint16_t id;
	uint8_t u, v;
} Texture_MapIcon;

typedef struct {
	C3D_Tex texture;
	Texture_MapIcon icons[TEXTURE_MAPTILES];
} Texture_Map;

#endif  // !TEXTURE_H_INCLUDED