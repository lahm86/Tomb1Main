#include "game/anims/frames.h"

#include "benchmark.h"
#include "debug.h"
#include "game/anims/common.h"
#include "game/gamebuf.h"
#include "game/objects/common.h"
#include "log.h"

#define EXTRACT_ROT_X(r) (((r & 0x3FF0) >> 4) << 6)
#define EXTRACT_ROT_Y(r1, r2) ((((r1 & 0xF) << 6) | ((r2 & 0xFC00) >> 10)) << 6)
#define EXTRACT_ROT_Z(r) ((r & 0x3FF) << 6)
#if TR_VERSION > 1
    #define EXTRACT_ROT_MODE(r) ((r & 0xC000) >> 14)
    #define EXTRACT_ONE_ROT(r) EXTRACT_ROT_Z(r)
#endif

#if TR_VERSION > 1
typedef enum {
    RPM_ALL = 0,
    RPM_X = 1,
    RPM_Y = 2,
    RPM_Z = 3,
} ROT_PACK_MODE;
#endif

static int32_t m_AnimCount = 0;
static int32_t m_FrameDataLength = 0;
static FRAME_INFO *m_Frames = NULL;

static OBJECT *M_GetAnimObject(int32_t anim_idx);
static int32_t M_GetAnimFrameCount(int32_t anim_idx);
static int32_t M_ParseFrame(
    FRAME_INFO *frame, const int16_t *data_ptr, int16_t num_meshes);
static int32_t M_ParseMeshRot(XYZ_16 *rot, const int16_t *data_ptr);

static OBJECT *M_GetAnimObject(const int32_t anim_idx)
{
    for (int32_t i = 0; i < O_NUMBER_OF; i++) {
        OBJECT *const object = Object_GetObject(i);
        if (object->loaded && object->mesh_count >= 0
            && object->anim_idx == anim_idx) {
            return object;
        }
    }

    return NULL;
}

static int32_t M_GetAnimFrameCount(const int32_t anim_idx)
{
    const ANIM *const anim = Anim_GetAnim(anim_idx);
#if TR_VERSION == 1
    return (int32_t)ceil(
        ((anim->frame_end - anim->frame_base) / (float)anim->interpolation)
        + 1);
#else
    uint32_t next_ofs = anim_idx == m_AnimCount - 1
        ? (unsigned)(m_FrameDataLength * sizeof(int16_t))
        : Anim_GetAnim(anim_idx + 1)->frame_ofs;
    return (next_ofs - anim->frame_ofs)
        / (int32_t)(sizeof(int16_t) * (anim->interpolation >> 8));
#endif
}

static int32_t M_ParseFrame(
    FRAME_INFO *const frame, const int16_t *data_ptr, const int16_t num_meshes)
{
    const int16_t *const frame_start = data_ptr;

    frame->bounds.min.x = *data_ptr++;
    frame->bounds.max.x = *data_ptr++;
    frame->bounds.min.y = *data_ptr++;
    frame->bounds.max.y = *data_ptr++;
    frame->bounds.min.z = *data_ptr++;
    frame->bounds.max.z = *data_ptr++;
    frame->offset.x = *data_ptr++;
    frame->offset.y = *data_ptr++;
    frame->offset.z = *data_ptr++;
#if TR_VERSION == 1
    ASSERT(num_meshes == *data_ptr++);
#endif

    frame->mesh_rots =
        GameBuf_Alloc(sizeof(XYZ_16) * num_meshes, GBUF_ANIM_FRAMES);
    for (int32_t i = 0; i < num_meshes; i++) {
        XYZ_16 *const rot = &frame->mesh_rots[i];
        data_ptr += M_ParseMeshRot(rot, data_ptr);
    }

#if TR_VERSION > 1
    // TR2 frames are aligned so we need to skip padding.
    data_ptr += MAX(0, (anim->interpolation >> 8) - (data_ptr - frame_start));
#endif
    return data_ptr - frame_start;
}

static int32_t M_ParseMeshRot(XYZ_16 *const rot, const int16_t *data_ptr)
{
#if TR_VERSION == 1
    const int16_t rot_val2 = *data_ptr++;
    const int16_t rot_val1 = *data_ptr++;
    rot->x = EXTRACT_ROT_X(rot_val1);
    rot->y = EXTRACT_ROT_Y(rot_val1, rot_val2);
    rot->z = EXTRACT_ROT_Z(rot_val2);
    return 2;
#else
    const int16_t rot_val1 = *data_ptr++;
    const ROT_PACK_MODE mode = EXTRACT_ROT_MODE(rot_val1);
    switch (mode) {
    case RPM_X:
        rot->x = EXTRACT_ONE_ROT(rot_val1);
        return 1;
    case RPM_Y:
        rot->y = EXTRACT_ONE_ROT(rot_val1);
        return 1;
    case RPM_Z:
        rot->z = EXTRACT_ONE_ROT(rot_val1);
        return 1;
    default:
        const int16_t rot_val1 = *data_ptr++;
        rot->x = EXTRACT_ROT_X(rot_val1);
        rot->y = EXTRACT_ROT_Y(rot_val1, rot_val2);
        rot->z = EXTRACT_ROT_Z(rot_val2);
        return 2;
    }
#endif
}

void Anim_ParseFrames(
    const int16_t *data, const int32_t data_length, const int32_t anim_count)
{
    BENCHMARK *const benchmark = Benchmark_Start();

    m_AnimCount = anim_count;
    m_FrameDataLength = data_length;

    int32_t total_frame_count = 0;
    int32_t anim_frame_counts[anim_count];
    for (int32_t i = 0; i < anim_count; i++) {
        const int32_t frame_count = M_GetAnimFrameCount(i);
        total_frame_count += frame_count;
        anim_frame_counts[i] = frame_count;
    }

    LOG_INFO("%d anim frames", total_frame_count);

    m_Frames =
        GameBuf_Alloc(sizeof(FRAME_INFO) * total_frame_count, GBUF_ANIM_FRAMES);

    OBJECT *cur_obj = NULL;
    int32_t frame_idx = 0;
    for (int32_t i = 0; i < anim_count; i++) {
        OBJECT *const next_obj = M_GetAnimObject(i);
        const bool obj_changed = next_obj != NULL;
        if (obj_changed) {
            cur_obj = next_obj;
        }

        if (cur_obj == NULL) {
            continue;
        }

        ANIM *const anim = Anim_GetAnim(i);
        const int16_t *data_ptr = &data[anim->frame_ofs / sizeof(int16_t)];
        for (int32_t j = 0; j < anim_frame_counts[i]; j++) {
            FRAME_INFO *const frame = Anim_GetFrame(frame_idx++);
            if (j == 0) {
                anim->frame_ptr = frame;
                if (obj_changed) {
                    cur_obj->frame_base = frame;
                }
            }

            data_ptr += M_ParseFrame(frame, data_ptr, cur_obj->mesh_count);
        }
    }

    Benchmark_End(benchmark, NULL);
}

FRAME_INFO *Anim_GetFrame(const int32_t frame_idx)
{
    return &m_Frames[frame_idx];
}
