#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <citro3d.h>
#include <3ds.h>

#include "stb_image/stb_image.h"

#include "craftus/render/render.h"

#include "craftus/render/texture.h"
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
static world_vertex* cursorVBO;

static Camera camera;

static C3D_Tex texture_map;

static C3D_RenderTarget *target, *rightTarget;

void Render_Init() {
	gfxSet3D(true);

	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);

	target = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24);
	C3D_RenderTargetSetClear(target, C3D_CLEAR_ALL, 0x68B0D8FF, 0);
	C3D_RenderTargetSetOutput(target, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);

	rightTarget = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24);
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

	C3D_TexEnv* env = C3D_GetTexEnv(0);
	C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, 0, 0);
	C3D_TexEnvOp(env, C3D_Both, 0, 0, 0);
	C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

	Camera_Init(&camera);

	Texture_Load(&texture_map, "romfs:/textures/sprites.png");

	extern world_vertex cube_sides_lut[36];  // Richtig schlechter stil. Ich muss dagegen noch was unternehmen

	cursorVBO = linearAlloc(sizeof(cube_sides_lut));
	memcpy(cursorVBO, cube_sides_lut, sizeof(cube_sides_lut));
	for (int i = 0; i < 36; i++) {
		cursorVBO[i].yuvb[3] = 30;
	}
	GX_FlushCacheRegions(cursorVBO, sizeof(cube_sides_lut), NULL, 0, NULL, 0);
}

void Render_Exit() {
	linearFree(cursorVBO);

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

static void drawWorld(Player* player) {
	int chunksDrawn = 0;

	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, world_shader_uLocModelView, &camera.view);

	const int turnTable[4][2] = {{-1, 0}, {0, -1}, {1, 0}, {0, 1}};

	ChunkCache* cache = player->cache;

	int yStart = (int)player->y / CHUNK_CLUSTER_HEIGHT;
	// TODO: Multthreading für Chunk polygonisierung
	int length = 1;
	int pX = CACHE_SIZE / 2, pZ = CACHE_SIZE / 2;
	int sX = 0, sZ = 1;
	int flipper = 2;
	int turn = 0;
	int totalTurn = 0;
	while (true) {  // Zeichnet die Chunks in Schlangenform und dadurch automatisch die vordersten zuerst
		// if (pX == 0 || pZ == 0 || pX == CACHE_SIZE - 1 || pZ == CACHE_SIZE - 1) break;  // Renderdistanz runterschrauben
		for (int i = 0; i < length; i++) {
			Chunk* c = cache->cache[pX][pZ];
			float chunkX = c->x * CHUNK_WIDTH, chunkZ = c->z * CHUNK_DEPTH;
			float distXZSqr = (chunkX - player->x) * (chunkX - player->x) + (chunkZ - player->z) * (chunkZ - player->z);
			if (distXZSqr <= ((M_PI + 0.5f - fabsf(player->pitch)) * 16.f * (M_PI + 0.5f - fabsf(player->pitch)) * 16.f))
				if (Camera_IsAABBVisible(&camera, (C3D_FVec){1.f, chunkZ, 0.f, chunkX}, (C3D_FVec){1.f, CHUNK_DEPTH, CHUNK_HEIGHT, CHUNK_WIDTH})) {
					bool passed = false;

					for (int j = 0; j < CHUNK_CLUSTER_COUNT; j++) {
						float distYSqr = j * CHUNK_CLUSTER_HEIGHT - player->y;
						if (c->data[j].vbo && c->data[j].vertexCount && distYSqr <= (fabsf(player->pitch) + 0.5f) * 16.f) {
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

	// printf("%d Chunks drawn", chunksDrawn);
}

static void drawCrossHair() {}

static void drawCursor(Player* player) {
	C3D_TexEnv* env = C3D_GetTexEnv(0);
	C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, 0, 0);
	C3D_TexEnvOp(env, C3D_Both, 0, 0, 0);
	C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

	C3D_Mtx modelView, modelMatrix;
	Mtx_Identity(&modelMatrix);
	Mtx_Translate(&modelMatrix, player->seightX, player->seightY, player->seightZ, true);
	Mtx_Scale(&modelMatrix, 1.07f, 1.07f, 1.07f);

	Mtx_Multiply(&modelView, &camera.view, &modelMatrix);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, world_shader_uLocModelView, &modelView);

	C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ONE, GPU_ONE, GPU_ONE);

	C3D_BufInfo bufInfo;
	BufInfo_Init(&bufInfo);
	BufInfo_Add(&bufInfo, cursorVBO, sizeof(world_vertex), 2, 0x10);
	C3D_SetBufInfo(&bufInfo);

	C3D_DrawArrays(GPU_TRIANGLES, 0, 36);

	C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);

	env = C3D_GetTexEnv(0);
	C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0);
	C3D_TexEnvOp(env, C3D_Both, 0, 0, 0);
	C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
}

void Render(Player* player) {
	C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

	polygonizeWorld(player->world);

	float iod = osGet3DSliderState() / 3.f;

	C3D_FrameDrawOn(target);

	Camera_Update(&camera, player, -iod);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, world_shader_uLocProjection, &camera.projection);

	drawWorld(player);

	drawCursor(player);

	if (iod > 0.f) {
		C3D_FrameDrawOn(rightTarget);

		Camera_Update(&camera, player, iod);
		C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, world_shader_uLocProjection, &camera.projection);

		drawWorld(player);

		drawCursor(player);
	}

	C3D_FrameEnd(0);
}