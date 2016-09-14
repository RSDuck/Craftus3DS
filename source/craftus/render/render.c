#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <3ds.h>
#include <citro3d.h>

#include "craftus/render/render.h"
#include "craftus/render/vbocache.h"

#include "craftus/entity/player.h"
#include "craftus/render/camera.h"
#include "craftus/render/texturemap.h"
#include "craftus/world/direction.h"
#include "craftus/world/world.h"

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

static Texture_Map texturemap;
static C3D_Tex iconstex;

static C3D_RenderTarget *target, *rightTarget;

void Render_Init() {
	gfxSet3D(true);

	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE * 2);

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

	C3D_BufInfo* bufInfo = C3D_GetBufInfo();
	BufInfo_Init(bufInfo);

	C3D_TexEnv* env = C3D_GetTexEnv(0);
	C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0);
	C3D_TexEnvOp(env, C3D_Both, 0, 0, 0);
	C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);

	Camera_Init(&camera);

	Blocks_InitTexture(&texturemap);

	Texture_Load(&iconstex, "romfs:/textures/gui/icons.png");

	extern world_vertex cube_sides_lut[36];  // Richtig schlechter stil. Ich muss dagegen noch was unternehmen

	cursorVBO = linearAlloc(sizeof(cube_sides_lut));
	memcpy(cursorVBO, cube_sides_lut, sizeof(cube_sides_lut));
	for (int i = 0; i < 36; i++) {
		cursorVBO[i].yuvb[3] = 20;
	}
	GX_FlushCacheRegions((u32*)cursorVBO, sizeof(cube_sides_lut), NULL, 0, NULL, 0);

	VBOCache_Init();
}

void Render_Exit() {
	VBOCache_Free();

	linearFree(cursorVBO);

	C3D_TexDelete(&texturemap.texture);
	C3D_TexDelete(&iconstex);

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
				if (BlockRender_PolygonizeChunk(world, c, true)) {
					c->flags &= ~ClusterFlags_VBODirty;
					return;
				}
			}
		}
	}
}

static void drawWorld(Player* player) {
	C3D_TexBind(0, &texturemap.texture);

	int chunksDrawn = 0;

	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, world_shader_uLocModelView, &camera.view);

	const int turnTable[4][2] = {{-1, 0}, {0, -1}, {1, 0}, {0, 1}};

	ChunkCache* cache = player->cache;

	int verticesTotal = 0;

	int yStart = (int)player->y / CHUNK_CLUSTER_HEIGHT;
	// TODO: Multthreading für Chunk polygonisierung
	int length = 1;
	int pX = CACHE_SIZE / 2, pZ = CACHE_SIZE / 2;
	int sX = 0, sZ = 1;
	int flipper = 2;
	int turn = 0;
	int totalTurn = 0;
	while (true) {  // Zeichnet die Chunks in Schlangenform und dadurch automatisch tsie vordersten zuerst
		for (int i = 0; i < length; i++) {
			Chunk* c = cache->cache[pX][pZ];
			float chunkX = c->x * CHUNK_WIDTH, chunkZ = c->z * CHUNK_DEPTH;
			float distXZSqr = (chunkX - player->x) * (chunkX - player->x) + (chunkZ - player->z) * (chunkZ - player->z);
			if (!(c->taskPending > 0) &&
			    distXZSqr <= (3.f * 16.f * 3.f * 16.f) /*((M_PI + 1.f - fabsf(player->pitch)) * 16.f * (M_PI + 1.f - fabsf(player->pitch)) * 16.f)*/)
				if (Camera_IsAABBVisible(&camera, (C3D_FVec){1.f, chunkZ, 0.f, chunkX}, (C3D_FVec){1.f, CHUNK_DEPTH, CHUNK_HEIGHT, CHUNK_WIDTH})) {
					bool passed = false;

					C3D_BufInfo bufInfo;
					BufInfo_Init(&bufInfo);

					for (int k = -1; k < 2; k += 2)
						for (int j = (FastFloor(player->y) < 0.f) ? 0.f : FastFloor(player->y); (j < CHUNK_CLUSTER_COUNT && k == 1) || (j >= 0 && k == -1);
						     j += k) {
							float distYSqr = j * CHUNK_CLUSTER_HEIGHT - player->y;
							if (c->data[j].vbo.memory && c->data[j].vertexCount && distYSqr <= (4.f * 16.f)) {
								bool visible = Camera_IsAABBVisible(&camera, (C3D_FVec){1.f, chunkZ, j * CHUNK_CLUSTER_HEIGHT, chunkX},
												    (C3D_FVec){1.f, CHUNK_DEPTH, CHUNK_CLUSTER_HEIGHT, CHUNK_WIDTH});
								if (visible) {
									BufInfo_Add(&bufInfo, c->data[j].vbo.memory, sizeof(world_vertex), 2, 0x10);

									C3D_SetBufInfo(&bufInfo);

									C3D_DrawArrays(GPU_TRIANGLES, 0, c->data[j].vertexCount);

									verticesTotal += c->data[j].vertexCount;

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

	// printf("%d vertices\n", verticesTotal);
}

static inline void immWorldVtx(float x, float y, float z, float u, float v, float brightness) {
	C3D_ImmSendAttrib(x, z, 0.f, 0.f);
	C3D_ImmSendAttrib(y, u, v, brightness);
}

static void drawTexture(C3D_Tex* tex, float x, float y, float subX, float subY, float subWidth, float subHeight) {
	float actualW = (subWidth * (1.f / 255.f)) * tex->width;
	float actualH = (subWidth * (1.f / 255.f)) * tex->height;

	const float depth = 0.f;

	C3D_ImmDrawBegin(GPU_TRIANGLES);

	immWorldVtx(x, y, depth, subX, subY, 255.f);
	immWorldVtx(x + actualW, y, depth, subX + subWidth, subY, 255.f);
	immWorldVtx(x + actualW, y + actualH, depth, subX + subWidth, subY + subHeight, 255.f);

	immWorldVtx(x, y + actualH, depth, subX, subY + subHeight, 255.f);
	immWorldVtx(x, y, depth, subX, subY, 255.f);
	immWorldVtx(x + actualW, y + actualH, depth, subX + subWidth, subY + subHeight, 255.f);

	C3D_ImmDrawEnd();
}

static void drawGUI(Player* player, float iod) {
	C3D_TexBind(0, &iconstex);
	C3D_Mtx projMtx, ident;

	Mtx_Identity(&ident);
	Mtx_OrthoTilt(&projMtx, 0.f, 200.f, 0.f, 120.f, 0.f, 1.f, false);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, world_shader_uLocProjection, &projMtx);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, world_shader_uLocModelView, &ident);

	C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_ALL);

	if (!(iod > 0.f)) {
		drawTexture(&iconstex, 200.f / 2.f - 8.f, 120.f / 2.f - 8.f, 0.f, 0.f, 16.f, 16.f);
	}

	C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL);
}

static void drawCursor(Player* player) {
	if (player->seightY != PLAYER_SEIGHT_INF) {
		C3D_TexEnv* env = C3D_GetTexEnv(0);
		C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, 0, 0);
		C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

		static Direction lastDirection = Direction_Back;

		if (lastDirection != player->seightDirection) {
			for (int i = 0; i < Directions_Count; i++) {
				if (i == player->seightDirection)
					for (int j = 0; j < 6; j++) cursorVBO[i * 6 + j].yuvb[3] = 90;
				if (i == lastDirection)
					for (int j = 0; j < 6; j++) cursorVBO[i * 6 + j].yuvb[3] = 20;
			}
			lastDirection = player->seightDirection;
		}

		C3D_Mtx modelView, modelMatrix;
		Mtx_Identity(&modelMatrix);
		Mtx_Translate(&modelMatrix, player->seightX - 0.035f, player->seightY - 0.035f, player->seightZ - 0.035f, true);
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
		C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
	}
}

void Render(Player* player) {
	if (C3D_FrameBegin(C3D_FRAME_SYNCDRAW)) {
		polygonizeWorld(player->world);

		float iod = osGet3DSliderState() / 3.f;

		C3D_FrameDrawOn(target);

		Camera_Update(&camera, player, -iod);
		C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, world_shader_uLocProjection, &camera.projection);

		drawWorld(player);

		drawCursor(player);

		drawGUI(player, iod);

		if (iod > 0.f) {
			C3D_FrameDrawOn(rightTarget);

			Camera_Update(&camera, player, iod);
			C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, world_shader_uLocProjection, &camera.projection);

			drawWorld(player);

			drawCursor(player);

			drawGUI(player, iod);
		}

		C3D_FrameEnd(0);
	}
}