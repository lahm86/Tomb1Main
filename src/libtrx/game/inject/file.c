#include "game/inject/file.h"

#include "benchmark.h"
#include "game/inject/common.h"
#include "log.h"
#include "memory.h"
#include "utils.h"

#include <assert.h>

#define INJECTION_MAGIC MKTAG('T', '0' + TR_VERSION, 'X', 'J')
#define INJECTION_CURRENT_VERSION 1

static int32_t m_NumInjections = 0;
static INJECTION *m_Injections = NULL;
static INJECT_INTERFACE *m_Interface = NULL;

// TODO: move to common
static int32_t m_DataCounts[IDT_NUMBER_OF] = { 0 };
static int32_t m_RoomMetaCount = 0;
static INJECTION_MESH_META *m_RoomMeta = NULL;

static void M_LoadFromFile(INJECTION *injection, const char *filename);
static void M_InitialiseBlock(VFILE *file);

static void M_LoadFromFile(
    INJECTION *const injection, const char *const filename)
{
    injection->relevant = false;

    VFILE *const fp = VFile_CreateFromPath(filename);
    injection->fp = fp;
    if (fp == NULL) {
        LOG_WARNING("Could not open %s", filename);
        return;
    }

    const uint32_t magic = VFile_ReadU32(fp);
    if (magic != INJECTION_MAGIC) {
        LOG_WARNING("Invalid injection magic in %s", filename);
        return;
    }

    injection->version = VFile_ReadS32(fp);
    if (injection->version < INJ_VERSION_1
        || injection->version > INJECTION_CURRENT_VERSION) {
        LOG_WARNING(
            "%s uses unsupported version %d", filename, injection->version);
        return;
    }

    injection->type = VFile_ReadS32(fp);
    if (injection->type < 0 || injection->type >= IMT_NUMBER_OF) {
        LOG_WARNING("%s is of unknown type %d", filename, injection->type);
        return;
    }

    injection->relevant = m_Interface->is_relevant(injection);
    if (!injection->relevant) {
        return;
    }

    const size_t start_pos = VFile_GetPos(fp);
    const int32_t num_blocksets = VFile_ReadS32(fp);

    for (int32_t i = 0; i < num_blocksets; i++) {
        VFile_Skip(fp, sizeof(int32_t) * 2);
        const int32_t num_blocks = VFile_ReadS32(fp);
        for (int32_t j = 0; j < num_blocks; j++) {
            M_InitialiseBlock(fp);
        }
    }

    VFile_SetPos(fp, start_pos);
    LOG_INFO("%s queued for injection", filename);
}

static void M_InitialiseBlock(VFILE *const file)
{
    const INJECTION_DATA_TYPE block_type = VFile_ReadS32(file);
    const int32_t data_count = VFile_ReadS32(file);
    const int32_t data_size = VFile_ReadS32(file);
    // TODO: make a handler in common to store the extra info
    m_DataCounts[block_type] += data_count;

    switch (block_type) {
    case IDT_ROOM_EDIT_META:
        m_RoomMetaCount = data_count;
        m_RoomMeta =
            Memory_Alloc(sizeof(INJECTION_MESH_META) * m_RoomMetaCount);
        for (int32_t i = 0; i < m_RoomMetaCount; i++) {
            INJECTION_MESH_META *const meta = &m_RoomMeta[i];
            meta->room_index = VFile_ReadS16(file);
            meta->num_vertices = VFile_ReadS16(file);
            meta->num_quads = VFile_ReadS16(file);
            meta->num_triangles = VFile_ReadS16(file);
            meta->num_sprites = VFile_ReadS16(file);
        }
        return;

    case IDT_SAMPLE_INFOS:
        for (int32_t i = 0; i < data_count; i++) {
            VFile_Skip(file, 3 * sizeof(int16_t));
            const int16_t flags = VFile_ReadS16(file);
            const int16_t num_samples = (flags >> 2) & 0xF;
            m_DataCounts[IDT_SAMPLE_INDICES] += num_samples;
#if TR_VERSION == 1
            for (int32_t j = 0; j < num_samples; j++) {
                const int32_t sample_length = VFile_ReadS32(file);
                m_DataCounts[IDT_SAMPLE_DATA] += sample_length;
                VFile_Skip(file, sizeof(char) * sample_length);
            }
#endif
        }
        return;

    case IDT_ANIM_FRAMES:
        // TODO: change file format to use structures
        m_DataCounts[IDT_ANIM_FRAMES] -= data_count;
        const size_t frame_data_start = VFile_GetPos(file);
        const size_t frame_data_end =
            frame_data_start + data_count * sizeof(int16_t);
        while (VFile_GetPos(file) < frame_data_end) {
            VFile_Skip(file, 9 * sizeof(int16_t));
            const int16_t num_meshes = VFile_ReadS16(file);
            VFile_Skip(file, num_meshes * sizeof(int32_t));
            m_DataCounts[IDT_ANIM_FRAMES]++;
            m_DataCounts[IDT_ANIM_FRAME_ROTS] += num_meshes;
        }
        return;

    default:
        break;
    }

    VFile_Skip(file, data_size);
}

int32_t Inject_GetDataCount(const INJECTION_DATA_TYPE type)
{
    return m_DataCounts[type];
}

const INJECT_INTERFACE *Inject_GetInterface(void)
{
    return m_Interface;
}

void Inject_Init(const INJECTION_ARGS *const args)
{
    m_NumInjections = args->num_files;
    if (m_NumInjections == 0) {
        return;
    }

    BENCHMARK *const benchmark = Benchmark_Start();

    m_Interface = Memory_Alloc(sizeof(INJECT_INTERFACE));
    memcpy(m_Interface, args->interface, sizeof(INJECT_INTERFACE));

    m_Injections = Memory_Alloc(sizeof(INJECTION) * m_NumInjections);

    for (int32_t i = 0; i < m_NumInjections; i++) {
        M_LoadFromFile(&m_Injections[i], args->files[i]);
    }

    Benchmark_End(benchmark, NULL);
}

void Inject_AllInjections(void)
{
    if (m_Injections == NULL) {
        return;
    }

    BENCHMARK *const benchmark = Benchmark_Start();

    for (int32_t i = 0; i < m_NumInjections; i++) {
        INJECTION *const injection = &m_Injections[i];
        if (!injection->relevant) {
            continue;
        }

        const int32_t num_blocksets = VFile_ReadS32(injection->fp);
        for (int32_t j = 0; j < num_blocksets; j++) {
            const INJECTION_SET_TYPE set_type = VFile_ReadS32(injection->fp);
            const int32_t set_size = VFile_ReadS32(injection->fp);
            const int32_t num_blocks = VFile_ReadS32(injection->fp);

            const INJECTION_SET set = {
                .injection = injection,
                .type = set_type,
                .num_blocks = num_blocks,
            };
            if (!Inject_InterpretSet(&set)) {
                LOG_WARNING("Unknown blockset type %d", set_type);
                VFile_Skip(injection->fp, set_size);
            }
        }

        // assert(VFile_GetPos(injection->fp) == injection->fp->size);
    }

    // Do texture packing if needed

    Benchmark_End(benchmark, NULL);
}

void Inject_Cleanup(void)
{
    if (m_Injections == NULL) {
        return;
    }

    BENCHMARK *const benchmark = Benchmark_Start();

    for (int32_t i = 0; i < m_NumInjections; i++) {
        const INJECTION *const injection = &m_Injections[i];
        if (injection->fp != NULL) {
            VFile_Close(injection->fp);
        }
    }

    for (int32_t i = 0; i < IDT_NUMBER_OF; i++) {
        m_DataCounts[i] = 0;
    }

    Memory_FreePointer(&m_Interface);
    Memory_FreePointer(&m_Injections);
    Memory_FreePointer(&m_RoomMeta);

    m_NumInjections = 0;
    m_RoomMetaCount = 0; // TODO move
    m_Injections = NULL;
    m_RoomMeta = NULL; // TODO move
    m_Interface = NULL;

    Benchmark_End(benchmark, NULL);
}

// TODO: move this and its data to common
INJECTION_MESH_META Inject_GetRoomMeshMeta(const int32_t room_index)
{
    INJECTION_MESH_META summed_meta = { 0 };
#if TR_VERSION == 1
    if (m_Injections == NULL || m_RoomMetaCount == 0) {
        return summed_meta;
    }

    for (int32_t i = 0; i < m_NumInjections; i++) {
        const INJECTION *const injection = &m_Injections[i];
        if (!injection->relevant) {
            continue;
        }

        for (int32_t j = 0; j < m_RoomMetaCount; j++) {
            const INJECTION_MESH_META *const meta = &m_RoomMeta[j];
            if (meta->room_index != room_index) {
                continue;
            }

            summed_meta.num_vertices += meta->num_vertices;
            summed_meta.num_quads += meta->num_quads;
            summed_meta.num_triangles += meta->num_triangles;
            summed_meta.num_sprites += meta->num_sprites;
        }
    }
#endif
    return summed_meta;
}
