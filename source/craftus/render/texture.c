#include "craftus/render/texture.h"

#include "stb_image/stb_image.h"

#include <3ds.h>

void Texture_Load(C3D_Tex* result, char* filename) {
	int w, h, fmt;
	u32* image = (u32)stbi_load(filename, &w, &h, &fmt, 4);
	for (int i = 0; i < w * h; i++) image[i] = __builtin_bswap32(image[i]);
	GSPGPU_FlushDataCache(image, w * h * 4);

	C3D_TexInitVRAM(result, w, h, GPU_RGBA8);
	C3D_TexSetFilter(result, GPU_NEAREST, GPU_NEAREST);

	GX_DisplayTransfer(image, GX_BUFFER_DIM(w, h), result->data, GX_BUFFER_DIM(w, h),
			   (GX_TRANSFER_FLIP_VERT(1) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) | GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) |
			    GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO)));

	stbi_image_free(image);
}