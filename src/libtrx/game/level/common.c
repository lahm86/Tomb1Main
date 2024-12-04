#include "game/level/common.h"

#include "benchmark.h"
#include "debug.h"
#include "game/gamebuf.h"
#include "game/inject.h"
#include "game/objects/common.h"
#include "game/rooms.h"
#include "log.h"
#include "memory.h"
#include "utils.h"
#include "vector.h"

static void M_ReadVertex(XYZ_16 *vertex, VFILE *file);
static void M_ReadFace4(FACE4 *face, VFILE *file);
static void M_ReadFace3(FACE3 *face, VFILE *file);
static void M_ReadObjectMesh(OBJECT_MESH *mesh, VFILE *file);

static void M_ReadVertex(XYZ_16 *const vertex, VFILE *const file)
{
    vertex->x = VFile_ReadS16(file);
    vertex->y = VFile_ReadS16(file);
    vertex->z = VFile_ReadS16(file);
}

static void M_ReadFace4(FACE4 *const face, VFILE *const file)
{
    for (int32_t i = 0; i < 4; i++) {
        face->vertices[i] = VFile_ReadU16(file);
    }
    face->texture = VFile_ReadU16(file);
    face->enable_reflections = false;
}

static void M_ReadFace3(FACE3 *const face, VFILE *const file)
{
    for (int32_t i = 0; i < 3; i++) {
        face->vertices[i] = VFile_ReadU16(file);
    }
    face->texture = VFile_ReadU16(file);
    face->enable_reflections = false;
}

static void M_ReadObjectMesh(OBJECT_MESH *const mesh, VFILE *const file)
{
    M_ReadVertex(&mesh->center, file);
    mesh->radius = VFile_ReadS32(file);

    mesh->enable_reflections = false;

    {
        mesh->num_vertices = VFile_ReadS16(file);
        mesh->vertices =
            GameBuf_Alloc(sizeof(XYZ_16) * mesh->num_vertices, GBUF_MESHES);
        for (int32_t i = 0; i < mesh->num_vertices; i++) {
            M_ReadVertex(&mesh->vertices[i], file);
        }
    }

    {
        mesh->num_lights = VFile_ReadS16(file);
        if (mesh->num_lights > 0) {
            mesh->lighting.normals =
                GameBuf_Alloc(sizeof(XYZ_16) * mesh->num_lights, GBUF_MESHES);
            for (int32_t i = 0; i < mesh->num_lights; i++) {
                M_ReadVertex(&mesh->lighting.normals[i], file);
            }
        } else {
            mesh->lighting.lights = GameBuf_Alloc(
                sizeof(int16_t) * ABS(mesh->num_lights), GBUF_MESHES);
            for (int32_t i = 0; i < ABS(mesh->num_lights); i++) {
                mesh->lighting.lights[i] = VFile_ReadS16(file);
            }
        }
    }

    {
        mesh->num_tex_face4s = VFile_ReadS16(file);
        mesh->tex_face4s =
            GameBuf_Alloc(sizeof(FACE4) * mesh->num_tex_face4s, GBUF_MESHES);
        for (int32_t i = 0; i < mesh->num_tex_face4s; i++) {
            M_ReadFace4(&mesh->tex_face4s[i], file);
        }
    }

    {
        mesh->num_tex_face3s = VFile_ReadS16(file);
        mesh->tex_face3s =
            GameBuf_Alloc(sizeof(FACE3) * mesh->num_tex_face3s, GBUF_MESHES);
        for (int32_t i = 0; i < mesh->num_tex_face3s; i++) {
            M_ReadFace3(&mesh->tex_face3s[i], file);
        }
    }

    {
        mesh->num_flat_face4s = VFile_ReadS16(file);
        mesh->flat_face4s =
            GameBuf_Alloc(sizeof(FACE4) * mesh->num_flat_face4s, GBUF_MESHES);
        for (int32_t i = 0; i < mesh->num_flat_face4s; i++) {
            M_ReadFace4(&mesh->flat_face4s[i], file);
        }
    }

    {
        mesh->num_flat_face3s = VFile_ReadS16(file);
        mesh->flat_face3s =
            GameBuf_Alloc(sizeof(FACE3) * mesh->num_flat_face3s, GBUF_MESHES);
        for (int32_t i = 0; i < mesh->num_flat_face3s; i++) {
            M_ReadFace3(&mesh->flat_face3s[i], file);
        }
    }
}

void Level_ReadRoomMesh(const int32_t room_num, VFILE *const file)
{
#if TR_VERSION == 1
    ROOM *const room = Room_Get(room_num);
    const INJECTION_MESH_META inj_data = Inject_GetRoomMeshMeta(room_num);

    const uint32_t mesh_length = VFile_ReadU32(file);
    size_t start_pos = VFile_GetPos(file);

    {
        room->mesh.num_vertices = VFile_ReadS16(file);
        const int32_t alloc_count =
            room->mesh.num_vertices + inj_data.num_vertices;
        room->mesh.vertices =
            GameBuf_Alloc(sizeof(ROOM_VERTEX) * alloc_count, GBUF_ROOM_MESH);
        for (int32_t i = 0; i < room->mesh.num_vertices; i++) {
            ROOM_VERTEX *const vertex = &room->mesh.vertices[i];
            M_ReadVertex(&vertex->pos, file);
            vertex->shade = VFile_ReadU16(file);
            vertex->flags = 0;
        }
    }

    {
        room->mesh.num_face4s = VFile_ReadS16(file);
        const int32_t alloc_count = room->mesh.num_face4s + inj_data.num_quads;
        room->mesh.face4s =
            GameBuf_Alloc(sizeof(FACE4) * alloc_count, GBUF_ROOM_MESH);
        for (int32_t i = 0; i < room->mesh.num_face4s; i++) {
            M_ReadFace4(&room->mesh.face4s[i], file);
        }
    }

    {
        room->mesh.num_face3s = VFile_ReadS16(file);
        const int32_t alloc_count =
            room->mesh.num_face3s + inj_data.num_triangles;
        room->mesh.face3s =
            GameBuf_Alloc(sizeof(FACE4) * alloc_count, GBUF_ROOM_MESH);
        for (int32_t i = 0; i < room->mesh.num_face3s; i++) {
            M_ReadFace3(&room->mesh.face3s[i], file);
        }
    }

    {
        room->mesh.num_sprites = VFile_ReadS16(file);
        const int32_t alloc_count =
            room->mesh.num_sprites + inj_data.num_sprites;
        room->mesh.sprites =
            GameBuf_Alloc(sizeof(ROOM_SPRITE) * alloc_count, GBUF_ROOM_MESH);
        for (int32_t i = 0; i < room->mesh.num_sprites; i++) {
            ROOM_SPRITE *const sprite = &room->mesh.sprites[i];
            sprite->vertex = VFile_ReadU16(file);
            sprite->texture = VFile_ReadU16(file);
        }
    }

    const size_t total_read =
        (VFile_GetPos(file) - start_pos) / sizeof(int16_t);
    ASSERT(total_read == mesh_length);
#endif
}

void Level_ReadObjectMeshes(
    const int32_t num_indices, const int32_t *const indices, VFILE *const file)
{
    // Construct and store distinct meshes only e.g. Lara's hips are referenced
    // by several pointers as a dummy mesh.
    VECTOR *const unique_indices =
        Vector_CreateAtCapacity(sizeof(int32_t), num_indices);
    int32_t pointer_map[num_indices];
    for (int32_t i = 0; i < num_indices; i++) {
        const int32_t pointer = indices[i];
        const int32_t index = Vector_IndexOf(unique_indices, (void *)&pointer);
        if (index == -1) {
            pointer_map[i] = unique_indices->count;
            Vector_Add(unique_indices, (void *)&pointer);
        } else {
            pointer_map[i] = index;
        }
    }

    OBJECT_MESH *const meshes =
        GameBuf_Alloc(sizeof(OBJECT_MESH) * unique_indices->count, GBUF_MESHES);
    size_t start_pos = VFile_GetPos(file);
    for (int32_t i = 0; i < unique_indices->count; i++) {
        const int32_t pointer = *(const int32_t *)Vector_Get(unique_indices, i);
        VFile_SetPos(file, start_pos + pointer);
        M_ReadObjectMesh(&meshes[i], file);

        // The original data position is required for backward compatibility
        // with savegame files, specifically for Lara's mesh pointers.
        Object_SetMeshOffset(&meshes[i], pointer / 2);
    }

    for (int32_t i = 0; i < num_indices; i++) {
        Object_StoreMesh(&meshes[pointer_map[i]]);
    }

    LOG_INFO("%d unique meshes constructed", unique_indices->count);

    Vector_Free(unique_indices);
}

void Level_ReadAnims(
    const int32_t base_idx, const int32_t num_anims, VFILE *const file)
{
#if TR_VERSION == 1
    for (int32_t i = 0; i < num_anims; i++) {
        ANIM *const anim = Anim_GetAnim(base_idx + i);

        anim->frame_ofs = VFile_ReadU32(file);
        anim->interpolation = VFile_ReadS16(file);
        anim->current_anim_state = VFile_ReadS16(file);
        anim->velocity = VFile_ReadS32(file);
        anim->acceleration = VFile_ReadS32(file);
        anim->frame_base = VFile_ReadS16(file);
        anim->frame_end = VFile_ReadS16(file);
        anim->jump_anim_num = VFile_ReadS16(file);
        anim->jump_frame_num = VFile_ReadS16(file);
        anim->num_changes = VFile_ReadS16(file);
        anim->change_idx = VFile_ReadS16(file);
        anim->num_commands = VFile_ReadS16(file);
        anim->command_idx = VFile_ReadS16(file);
    }
#endif
}

void Level_ReadAnimChanges(
    const int32_t base_idx, const int32_t num_changes, VFILE *const file)
{
    for (int32_t i = 0; i < num_changes; i++) {
        ANIM_CHANGE *const anim_change = Anim_GetChange(base_idx + i);
        anim_change->goal_anim_state = VFile_ReadS16(file);
        anim_change->num_ranges = VFile_ReadS16(file);
        anim_change->range_idx = VFile_ReadS16(file);
    }
}

void Level_ReadAnimRanges(
    const int32_t base_idx, const int32_t num_ranges, VFILE *const file)
{
    for (int32_t i = 0; i < num_ranges; i++) {
        ANIM_RANGE *const anim_range = Anim_GetRange(base_idx + i);
        anim_range->start_frame = VFile_ReadS16(file);
        anim_range->end_frame = VFile_ReadS16(file);
        anim_range->link_anim_num = VFile_ReadS16(file);
        anim_range->link_frame_num = VFile_ReadS16(file);
    }
}

void Level_ReadAnimCommands(
    const int32_t base_idx, const int32_t num_cmds, VFILE *const file)
{
    // TODO: structure these, although they are of variable size.
    for (int32_t i = 0; i < num_cmds; i++) {
        int16_t *const cmd = Anim_GetCommand(base_idx + i);
        *cmd = VFile_ReadS16(file);
    }
}

void Level_ReadAnimBones(
    const int32_t base_idx, const int32_t num_bones, VFILE *const file)
{
    for (int32_t i = 0; i < num_bones; i++) {
        ANIM_BONE *const bone = Anim_GetBone(base_idx + i);
        bone->flags = VFile_ReadS32(file);
        bone->pos.x = VFile_ReadS32(file);
        bone->pos.y = VFile_ReadS32(file);
        bone->pos.z = VFile_ReadS32(file);
    }
}

static OBJECT *M_GetAnimObject(const int32_t anim_idx)
{
    for (int32_t i = 0; i < O_NUMBER_OF; i++) {
        OBJECT *const object = Object_GetObject(i);
        if (object->loaded && object->nmeshes >= 0
            && object->anim_idx == anim_idx) {
            return object;
        }
    }

    return NULL;
}

void Level_ReadAnimFrames(
    const int16_t *const data, const int32_t data_length,
    const int32_t anim_count)
{
    BENCHMARK *const benchmark = Benchmark_Start();

    const OBJECT *current_object = NULL;
    int32_t total_frame_count = 0;
    int32_t anim_frame_counts[anim_count];
    for (int32_t i = 0; i < anim_count; i++) {
        const ANIM *const anim = Anim_GetAnim(i);
#if TR_VERSION == 1
        const int32_t frame_count = (int32_t)ceil(
            ((anim->frame_end - anim->frame_base) / (float)anim->interpolation)
            + 1);
#else
        uint32_t next_ofs = i == anim_count - 1
            ? (unsigned)(data_length * sizeof(int16_t))
            : Anim_GetAnim(i + 1)->frame_ofs;
        const int32_t frame_count = (next_ofs - anim->frame_ofs)
            / (int32_t)(sizeof(int16_t) * (anim->interpolation >> 8));
#endif
        total_frame_count += frame_count;
        anim_frame_counts[i] = frame_count;
    }

    LOG_INFO("%d anim frames", total_frame_count);

    Anim_InitialiseFrames(total_frame_count);

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
#if TR_VERSION > 1
            const int16_t *const frame_start = data_ptr;
#endif
            FRAME_INFO *const frame = Anim_GetFrame(frame_idx++);
            if (j == 0) {
                anim->frame_ptr = frame;
                if (obj_changed) {
                    cur_obj->frame_base = frame;
                }
            }

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
            data_ptr++; // Skip num_meshes
#endif
            // NB rots will need to be parsed for TR2 as some are 2 bytes
            // others 4...
            frame->mesh_rots = GameBuf_Alloc(
                sizeof(int32_t) * cur_obj->nmeshes, GBUF_ANIM_FRAMES);
            memcpy(
                frame->mesh_rots, data_ptr, sizeof(int32_t) * cur_obj->nmeshes);
            data_ptr += cur_obj->nmeshes * sizeof(int32_t) / sizeof(int16_t);

#if TR_VERSION > 1
            // ...and hence TR2 frames are aligned so need to skip padding.
            data_ptr +=
                MAX(0, (anim->interpolation >> 8) - (data_ptr - frame_start));
#endif
        }
    }

    Benchmark_End(benchmark, NULL);
}
