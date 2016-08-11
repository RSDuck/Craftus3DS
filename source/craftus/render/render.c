#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <citro3d.h>

#include "stb_image/stb_image.h"

#include "craftus/render/render.h"

#include "craftus/world/direction.h"
#include "craftus/world/world.h"
#include "craftus/entity/player.h"
#include "craftus/render/camera.h"

#include "world_shbin.h"

#define DISPLAY_TRANSFER_FLAGS                                                                                                          \
	(GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | \
	 GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

static DVLB_s* world_dvlb;
static shaderProgram_s world_shader;
static int world_shader_uLocProjection;
static int world_shader_uLocModelView;

static Camera camera;

static C3D_Tex texture_map;

static C3D_RenderTarget *target, *rightTarget;

/*
Both of these(morton_interleave and get_morton_offset) seem to be more or less triple stolen from here: https://fgiesen.wordpress.com/2009/12/13/decoding-morton-codes/
I have them from sf2dlib
*/
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

void Render_Init() {
	gfxSet3D(true);

	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);

	target = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	C3D_RenderTargetSetClear(target, C3D_CLEAR_ALL, 0x68B0D8FF, 0);
	C3D_RenderTargetSetOutput(target, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);

	rightTarget = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	C3D_RenderTargetSetClear(rightTarget, C3D_CLEAR_ALL, 0x68B0D8FF, 0);
	C3D_RenderTargetSetOutput(rightTarget, GFX_TOP, GFX_RIGHT, DISPLAY_TRANSFER_FLAGS);

	world_dvlb = DVLB_ParseFile((u32*)world_shbin, world_shbin_size);
	shaderProgramInit(&world_shader);
	shaderProgramSetVsh(&world_shader, &world_dvlb->DVLE[0]);
	// shaderProgramSetGsh(&world_shader, &world_dvlb->DVLE[1], 5);
	C3D_BindProgram(&world_shader);

	world_shader_uLocProjection = shaderInstanceGetUniformLocation(world_shader.vertexShader, "projection");
	world_shader_uLocModelView = shaderInstanceGetUniformLocation(world_shader.vertexShader, "modelView");

	C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
	AttrInfo_Init(attrInfo);
	AttrInfo_AddLoader(attrInfo, 0, GPU_SHORT, 2);
	AttrInfo_AddLoader(attrInfo, 1, GPU_UNSIGNED_BYTE, 4);

	{
		int w, h, fmt;
		u8* image = stbi_load("romfs:/textures/sprites.png", &w, &h, &fmt, 4);

		u8* gpuImg = linearAlloc(w * h * 4);

		for (int j = 0; j < h + 16; j++) {  // TODO: Herausfinden warum + 16 hilft
			for (int i = 0; i < w; i++) {
				u32 coarse_y = j & ~7;
				u32 dst_offset = get_morton_offset(i, j, 4) + coarse_y * w * 4;

				u32 v = ((u32*)image)[i + (w * j)];
				*(u32*)(gpuImg + dst_offset) = __builtin_bswap32(v); /* RGBA8 -> ABGR8 */
			}
		}

		GSPGPU_FlushDataCache(gpuImg, w * h * 4);

		C3D_TexInit(&texture_map, w, h, GPU_RGBA8);
		C3D_TexUpload(&texture_map, gpuImg);
		C3D_TexSetFilter(&texture_map, GPU_NEAREST, GPU_NEAREST);
		C3D_TexSetWrap(&texture_map, GPU_REPEAT, GPU_REPEAT);
		C3D_TexBind(0, &texture_map);

		linearFree(gpuImg);
		stbi_image_free(image);
	}

	C3D_TexEnv* env = C3D_GetTexEnv(0);
	C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0);
	C3D_TexEnvOp(env, C3D_Both, 0, 0, 0);
	C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);

	C3D_CullFace(GPU_CULL_BACK_CCW);

	Camera_Init(&camera);
}

void Render_Exit() {
	C3D_TexDelete(&texture_map);

	shaderProgramFree(&world_shader);
	DVLB_Free(world_dvlb);

	C3D_Fini();
	gfxExit();
}

static void polygonizeWorld(World* world) {
	ChunkCache* cache = world->cache[0];
	for (int x = 0; x < CACHE_SIZE; x++) {
		for (int z = 0; z < CACHE_SIZE; z++) {
			Chunk* c = cache->cache[x][z];
			if (c->flags & ClusterFlags_VBODirty) {
				if (BlockRender_PolygonizeChunk(world, c)) {
					c->flags &= ~ClusterFlags_VBODirty;
					return;
				}
			}
		}
	}
}

static void renderWorld(Player* player) {
	C3D_Mtx modelMtx, modelView;

	int chunksDrawn = 0;

	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, world_shader_uLocModelView, &camera.view);

	const int turnTable[4][2] = {{-1, 0}, {0, -1}, {1, 0}, {0, 1}};

	ChunkCache* cache = player->cache;

	int yStart = (int)player->y / CHUNK_CLUSTER_HEIGHT;

	int length = 1;
	int pX = CACHE_SIZE / 2, pZ = CACHE_SIZE / 2;
	int sX = 0, sZ = 1;
	int flipper = 2;
	int turn = 0;
	int totalTurn = 0;
	while (true) {  // Zeichnet die Chunks in Schlangenform und dadurch automatisch die vordersten zuerst
		for (int i = 0; i < length; i++) {
			Chunk* c = cache->cache[pX][pZ];
			float chunkX = c->x * CHUNK_WIDTH, chunkZ = c->z * CHUNK_DEPTH;
			if (Camera_IsAABBVisible(&camera, (C3D_FVec){1.f, chunkZ, 0.f, chunkX}, (C3D_FVec){1.f, CHUNK_DEPTH, CHUNK_HEIGHT, CHUNK_WIDTH})) {
				bool passed = false;

				for (int j = 0; j < CHUNK_CLUSTER_COUNT; j++)
					if (c->data[j].vbo && c->data[j].vertexCount) {
						bool visible = Camera_IsAABBVisible(&camera, (C3D_FVec){1.f, chunkZ, j * CHUNK_CLUSTER_HEIGHT, chunkX},
										    (C3D_FVec){1.f, CHUNK_DEPTH, CHUNK_CLUSTER_HEIGHT, CHUNK_WIDTH});
						if (visible) {
							C3D_BufInfo bufInfo;
							BufInfo_Init(&bufInfo);
							BufInfo_Add(&bufInfo, c->data[j].vbo, sizeof(world_vertex), 2, 0x10);

							C3D_SetBufInfo(&bufInfo);

							C3D_DrawArrays(GPU_TRIANGLES, 0, c->data[j].vertexCount);

							passed = true;
						} else if (passed)
							break;
					}

				chunksDrawn++;
			}

			pX += sX;
			pZ += sZ;
		}
		sX = turnTable[turn][0];
		sZ = turnTable[turn][1];
		if (length == CACHE_SIZE) break;
		if (!(--flipper)) {
			length++;
			flipper = 2;
		}
		if (++turn >= 4) turn = 0;

		totalTurn++;
	}
}

void Render(Player* player) {
	C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
	C3D_FrameDrawOn(target);

	polygonizeWorld(player->world);

	float iod = osGet3DSliderState() / 3.f;

	Camera_Update(&camera, player, -iod);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, world_shader_uLocProjection, &camera.projection);

	renderWorld(player);

	if (iod > 0.f) {
		C3D_FrameDrawOn(rightTarget);

		Camera_Update(&camera, player, iod);
		C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, world_shader_uLocProjection, &camera.projection);

		renderWorld(player);
	}
	C3D_FrameEnd(0);
}