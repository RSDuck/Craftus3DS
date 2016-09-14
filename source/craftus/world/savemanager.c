#include "craftus/world/savemanager.h"

#include <3ds.h>

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "mpack/mpack.h"

#include "vec/vec.h"

#include "pithy/pithy.h"

typedef struct {
	int x, z;
	char* data;
	size_t size;
} savedChunk;

typedef struct {
	int x, z;
	u32 editCounter;
	size_t size;
	size_t filePosition;
	bool inUse;
} chunkEntry;

static long writingHead = 0;

static World* workingWorld;
static char manifestPath[64];
static char chunksPath[64];
static char indexPath[64];

static void* cacheMemForChunk;
static void* compressionBuffer;

static int serializedSizeInMem = 0;

static FILE* chunkFile;

static vec_t(chunkEntry) savedChunks;

static void saveIndex() {
	mpack_writer_t writer;
	mpack_writer_init_file(&writer, indexPath);

	mpack_start_map(&writer, 2);

	mpack_write_cstr(&writer, "writingHead");
	mpack_write_i32(&writer, writingHead);

	mpack_write_cstr(&writer, "chunks");
	mpack_start_array(&writer, savedChunks.length);
	for (int i = 0; i < savedChunks.length; i++) {
		mpack_start_map(&writer, 6);

		mpack_write_cstr(&writer, "x");
		mpack_write_i32(&writer, savedChunks.data[i].x);
		mpack_write_cstr(&writer, "z");
		mpack_write_i32(&writer, savedChunks.data[i].z);
		mpack_write_cstr(&writer, "filePosition");
		mpack_write_i32(&writer, savedChunks.data[i].filePosition);
		mpack_write_cstr(&writer, "editCounter");
		mpack_write_i32(&writer, savedChunks.data[i].editCounter);
		mpack_write_cstr(&writer, "inUse");
		mpack_write_bool(&writer, savedChunks.data[i].inUse);
		mpack_write_cstr(&writer, "size");
		mpack_write_i32(&writer, savedChunks.data[i].size);

		mpack_finish_map(&writer);
	}
	mpack_finish_array(&writer);

	mpack_finish_map(&writer);

	if (mpack_writer_destroy(&writer) != mpack_ok) {
		printf("Couldn't write chunk index to file\n");
	}
}

static void loadIndex() {
	mpack_tree_t tree;
	mpack_tree_init_file(&tree, indexPath, 0);
	mpack_node_t root = mpack_tree_root(&tree);

	writingHead = mpack_node_i32(mpack_node_map_cstr(root, "writingHead"));

	mpack_node_t chunks = mpack_node_map_cstr(root, "chunks");
	size_t length = mpack_node_array_length(chunks);
	for (int i = 0; i < length; i++) {
		mpack_node_t chunk = mpack_node_array_at(chunks, i);

		chunkEntry entry;
		entry.x = mpack_node_i32(mpack_node_map_cstr(chunk, "x"));
		entry.z = mpack_node_i32(mpack_node_map_cstr(chunk, "z"));
		entry.filePosition = mpack_node_i32(mpack_node_map_cstr(chunk, "filePosition"));
		entry.size = mpack_node_i32(mpack_node_map_cstr(chunk, "size"));
		entry.editCounter = mpack_node_i32(mpack_node_map_cstr(chunk, "editCounter"));
		entry.inUse = mpack_node_bool(mpack_node_map_cstr(chunk, "inUse"));

		vec_push(&savedChunks, entry);
	}

	int err = mpack_tree_destroy(&tree);
	if (err != mpack_ok) {
		printf("Error while loading chunk index %d\n", err);
	}
}

static void saveManifest() {
	mpack_writer_t writer;
	mpack_writer_init_file(&writer, manifestPath);

	mpack_start_map(&writer, 2);
	mpack_write_cstr(&writer, "seed");
	mpack_write_i64(&writer, workingWorld->genConfig.seed);
	mpack_write_cstr(&writer, "type");
	mpack_write_i8(&writer, workingWorld->genConfig.type);
	mpack_finish_map(&writer);

	if (mpack_writer_destroy(&writer) != mpack_ok) {
		printf("Couldn't write to file!\n");
	}
}

static savedChunk serializeChunk(Chunk* chunk, bool testRun) {
	mpack_writer_t writer;
	size_t size;
	char* ptr;
	if (testRun)
		mpack_writer_init_growable(&writer, &ptr, &size);
	else
		mpack_writer_init(&writer, cacheMemForChunk, serializedSizeInMem);

	mpack_start_map(&writer, 2);

	mpack_write_cstr(&writer, "cluster");
	mpack_start_array(&writer, CHUNK_CLUSTER_COUNT);
	for (int i = 0; i < CHUNK_CLUSTER_COUNT; i++) {
		mpack_start_map(&writer, 2);
		mpack_write_cstr(&writer, "empty");
		mpack_write_bool(&writer, !!(chunk->data[i].flags & ClusterFlags_Empty));
		mpack_write_cstr(&writer, "blocks");
		mpack_write_bin(&writer, (char*)chunk->data[i].blocks, sizeof(chunk->data[i].blocks));
		mpack_finish_map(&writer);
	}
	mpack_finish_array(&writer);

	mpack_write_cstr(&writer, "heightmap");
	mpack_write_bin(&writer, (char*)chunk->heightmap, sizeof(chunk->heightmap));

	mpack_finish_map(&writer);

	int err = mpack_writer_destroy(&writer);
	if (err != mpack_ok) {
		printf("Error while serializing chunk(%d)\n", err);
	}

	savedChunk out;
	out.x = chunk->x;
	out.z = chunk->z;
	out.data = testRun ? ptr : cacheMemForChunk;
	out.size = testRun ? size : serializedSizeInMem;

	return out;
}

static void deserializeChunk(savedChunk data, Chunk* out) {
	mpack_tree_t tree;

	mpack_tree_init(&tree, data.data, data.size);
	mpack_node_t root = mpack_tree_root(&tree);

	mpack_node_t clusters = mpack_node_map_cstr(root, "cluster");
	size_t length = mpack_node_array_length(clusters);
	for (int i = 0; i < length; i++) {
		mpack_node_t cluster = mpack_node_array_at(clusters, i);
		bool empty = mpack_node_bool(mpack_node_map_cstr(cluster, "empty"));
		if (!empty) {
			mpack_node_t node = mpack_node_map_cstr(cluster, "blocks");
			Block* blocks = (Block*)mpack_node_data(node);
			memcpy(out->data[i].blocks, blocks, sizeof(out->data[i].blocks));
			out->data[i].flags = (out->data[i].flags & ~ClusterFlags_Empty) | ClusterFlags_VBODirty;
		} else
			out->data[i].flags |= ClusterFlags_Empty;
	}
	out->flags |= ClusterFlags_VBODirty;

	mpack_node_t heightmap = mpack_node_map_cstr(root, "heightmap");
	memcpy(out->heightmap, mpack_node_data(heightmap), sizeof(out->heightmap));
	out->heightMapEdit = out->editsCounter;

	mpack_error_t err = mpack_tree_destroy(&tree);
	if (err != mpack_ok) {
		printf("Error %d while deserializing chunk(%d, %d)\n", err, out->x, out->z);
	}
}

void SaveManager_Init(World* world) {
	{
		Chunk* c = (Chunk*)malloc(sizeof(Chunk));
		savedChunk sC = serializeChunk(c, true);
		serializedSizeInMem = sC.size;
		free(sC.data);
		free(c);
		printf("%d, %d\n", serializedSizeInMem, sizeof(Chunk));
	}
	cacheMemForChunk = malloc(serializedSizeInMem);
	compressionBuffer = malloc(serializedSizeInMem);

	vec_init(&savedChunks);

	workingWorld = world;
	sprintf(manifestPath, "sdmc:/craftus/saves/%s/level.mpack", world->name);
	sprintf(chunksPath, "sdmc:/craftus/saves/%s/chunks.bin", world->name);
	sprintf(indexPath, "sdmc:/craftus/saves/%s/index.mpack", world->name);

	char worldDir[64];
	sprintf(worldDir, "sdmc:/craftus/saves", world->name);
	mkdir(worldDir, 777);
	sprintf(worldDir, "sdmc:/craftus/saves/%s", world->name);
	mkdir(worldDir, 777);

	mpack_tree_t tree;
	mpack_tree_init_file(&tree, manifestPath, 0);
	mpack_node_t root = mpack_tree_root(&tree);

	long seed = mpack_node_i64(mpack_node_map_cstr(root, "seed"));
	char type = mpack_node_i8(mpack_node_map_cstr(root, "type"));

	if (mpack_tree_destroy(&tree) != mpack_ok) {
		saveManifest();
		printf("Couldn't read manifest, creating a new one\n");
	} else {
		printf("Sucessfull read world manifest!\n");
		workingWorld->genConfig.seed = seed;
		workingWorld->genConfig.type = type;
	}

	printf("Gone through\n");

	chunkFile = fopen(chunksPath, "r+b");
	if (!chunkFile) {
		chunkFile = fopen(chunksPath, "w+b");
	}
	if (!chunkFile) {
		printf("Error, couldn't open file\n");
	}

	loadIndex();

	printf("Initialized save manager\n");
}

void SaveManager_Free() {
	SaveManager_Flush();
	saveManifest();

	saveIndex();

	vec_deinit(&savedChunks);
	free(compressionBuffer);
	free(cacheMemForChunk);
	fclose(chunkFile);
}

void SaveManager_Flush() { saveIndex(); }

bool SaveManager_SaveChunk(ChunkWorker_Queue* queue, ChunkWorker_Task task) {
	chunkEntry chunk;
	int i;
	savedChunk newChunk = serializeChunk(task.chunk, false);

	printf("Saving chunk(%d, %d)...\n", task.chunk->x, task.chunk->z);
	vec_foreach (&savedChunks, chunk, i) {
		if ((task.chunk->x == chunk.x && task.chunk->z == chunk.z) || !chunk.inUse) {
			if ((chunk.inUse && chunk.editCounter != task.chunk->editsCounter) || !chunk.inUse) {
				if (chunk.size >= newChunk.size) {
					fseek(chunkFile, chunk.filePosition, SEEK_SET);
					size_t written = fwrite(newChunk.data, 1, newChunk.size, chunkFile);
					savedChunks.data[i].editCounter = task.chunk->editsCounter;
					savedChunks.data[i].inUse = true;
					savedChunks.data[i].x = task.chunk->x;
					savedChunks.data[i].z = task.chunk->z;

					if (written != newChunk.size) {
						printf("!Warning! written memory does not match\n");
					}

					return true;
				} else {
					savedChunks.data[i].inUse = false;
				}
			}
		}
	}
	fseek(chunkFile, writingHead, SEEK_SET);

	size_t written = fwrite(newChunk.data, 1, newChunk.size, chunkFile);
	if (written != newChunk.size) {
		printf("!Warning! written memory does not match\n");
	}

	chunk.x = task.chunk->x;
	chunk.z = task.chunk->z;
	chunk.filePosition = writingHead;
	chunk.editCounter = task.chunk->editsCounter;
	chunk.size = newChunk.size;
	chunk.inUse = true;
	vec_push(&savedChunks, chunk);

	writingHead += newChunk.size;

	saveIndex();

	return true;
}

bool SaveManager_LoadChunk(ChunkWorker_Queue* queue, ChunkWorker_Task task) {
	chunkEntry chunk;
	int i;
	// printf("Loading chunk from file... ");
	vec_foreach (&savedChunks, chunk, i) {
		if (chunk.x == task.chunk->x && chunk.z == task.chunk->z && chunk.inUse) {
			fseek(chunkFile, chunk.filePosition, SEEK_SET);
			size_t bytesRead = fread(cacheMemForChunk, 1, chunk.size, chunkFile);

			if (bytesRead != chunk.size) {
				printf("!Warning! read memory does not match\n");
			}

			savedChunk savedChunk;
			savedChunk.x = chunk.x;
			savedChunk.z = chunk.z;
			savedChunk.size = chunk.size;
			savedChunk.data = cacheMemForChunk;
			task.chunk->editsCounter = chunk.editCounter;
			deserializeChunk(savedChunk, task.chunk);
			return true;
		}
	}
	// printf("Not found, generating\n");
	task.type = ChunkWorker_TaskDecorateChunk;
	task.chunk->tasksPending++;
	vec_push(queue, task);
	return true;
}
