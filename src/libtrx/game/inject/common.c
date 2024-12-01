#include "game/inject/common.h"

#include "benchmark.h"
#include "game/const.h"
#include "game/inject.h"
#include "game/items.h"
#include "game/objects/common.h"
#include "game/rooms.h"
#include "log.h"
#include "memory.h"
#include "utils.h"

#define NULL_FD_INDEX ((uint16_t)(-1))

static void M_InterpretSet(const INJECTION_SET *set);

static void M_ApplyFloorEdits(const INJECTION_SET *set, int32_t data_count);
static void M_TriggerTypeChange(
    const INJECTION *injection, const SECTOR *sector);
static void M_TriggerParameterChange(
    const INJECTION *injection, const SECTOR *sector);
static void M_SetMusicOneShot(const SECTOR *sector);
static void M_InsertFloorData(const INJECTION *injection, SECTOR *sector);
static void M_RoomShift(const INJECTION *injection, int16_t room_num);
static void M_RoomProperties(const INJECTION *injection, int16_t room_num);
static void M_TriggeredItem(const INJECTION *injection);

static void M_ApplyItemEdits(const INJECTION_SET *set, int32_t data_count);
static void M_ApplyPortalEdits(const INJECTION_SET *set, int32_t data_count);

static void M_ApplyObjectMeshEdits(
    const INJECTION_SET *set, int32_t data_count);
static void M_ApplyMeshEdit(const MESH_EDIT *mesh_edit);
static void M_ApplyFace4Edit(
    const FACE_EDIT *edit, FACE4 *faces, uint16_t texture);
static void M_ApplyFace3Edit(
    const FACE_EDIT *edit, FACE3 *faces, uint16_t texture);
static uint16_t *M_GetMeshTexture(const FACE_EDIT *face_edit);

static void M_ApplyRoomMeshEdits(const INJECTION_SET *set, int32_t data_count);
static void M_TextureRoomFace(const INJECTION *injection);
static void M_MoveRoomFace(const INJECTION *injection);
static void M_AlterRoomVertex(const INJECTION *injection);
static void M_RotateRoomFace(const INJECTION *injection);
static void M_AddRoomFace(const INJECTION *injection);
static void M_AddRoomVertex(const INJECTION *injection);
static void M_AddRoomSprite(const INJECTION *injection);
static uint16_t *M_GetRoomTexture(
    int16_t room_num, FACE_TYPE face_type, int16_t face_index);
static uint16_t *M_GetRoomFaceVertices(
    int16_t room_num, FACE_TYPE face_type, int16_t face_index);

static void M_InterpretSet(const INJECTION_SET *const set)
{
    VFILE *const file = set->injection->fp;
    for (int32_t i = 0; i < set->num_blocks; i++) {
        const INJECTION_DATA_TYPE block_type = VFile_ReadS32(file);
        const int32_t data_count = VFile_ReadS32(file);
        const int32_t data_size = VFile_ReadS32(file);

        switch (block_type) {
        case IDT_FLOOR_EDITS:
            M_ApplyFloorEdits(set, data_count);
            break;
        case IDT_ITEM_EDITS:
            M_ApplyItemEdits(set, data_count);
            break;
        case IDT_MESH_EDITS:
            M_ApplyObjectMeshEdits(set, data_count);
            break;
        case IDT_ROOM_EDITS:
            M_ApplyRoomMeshEdits(set, data_count);
            break;
        case IDT_VIS_PORTAL_EDITS:
            M_ApplyPortalEdits(set, data_count);
            break;
        default:
            VFile_Skip(file, data_size);
            break;
        }
    }
}

static void M_ApplyFloorEdits(
    const INJECTION_SET *const set, const int32_t data_count)
{
    BENCHMARK *const benchmark = Benchmark_Start();

    VFILE *const file = set->injection->fp;
    for (int32_t i = 0; i < data_count; i++) {
        const int16_t room_num = VFile_ReadS16(file);
        const uint16_t x = VFile_ReadU16(file);
        const uint16_t z = VFile_ReadU16(file);
        const int32_t fd_edit_count = VFile_ReadS32(file);

        // Verify that the given room and coordinates are accurate.
        // Individual FD functions must check that sector is actually set.
        const ROOM *room = NULL;
        SECTOR *sector = NULL;
        if (room_num < 0 || room_num >= Room_GetTotalCount()) {
            LOG_WARNING("Room index %d is invalid", room_num);
        } else {
            room = Room_Get(room_num);
            if (x >= room->size.x || z >= room->size.z) {
                LOG_WARNING(
                    "Sector [%d,%d] is invalid for room %d", x, z, room_num);
            } else {
                sector = &room->sectors[room->size.z * x + z];
            }
        }

        const INJECT_INTERFACE *const interface = Inject_GetInterface();
        for (int32_t j = 0; j < fd_edit_count; j++) {
            const FLOOR_EDIT_TYPE edit_type = VFile_ReadS32(file);
            if (interface->handle_floor_edit != NULL
                && interface->handle_floor_edit(edit_type, set->injection)) {
                continue;
            }

            switch (edit_type) {
            case FET_TRIGGER_TYPE:
                M_TriggerTypeChange(set->injection, sector);
                break;
            case FET_TRIGGER_PARAM:
                M_TriggerParameterChange(set->injection, sector);
                break;
            case FET_MUSIC_ONESHOT:
                M_SetMusicOneShot(sector);
                break;
            case FET_FD_INSERT:
                M_InsertFloorData(set->injection, sector);
                break;
            case FET_ROOM_SHIFT:
                M_RoomShift(set->injection, room_num);
                break;
            case FET_TRIGGER_ITEM:
                M_TriggeredItem(set->injection);
                break;
            case FET_ROOM_PROPERTIES:
                M_RoomProperties(set->injection, room_num);
                break;
            default:
                LOG_WARNING("Unknown floor data edit type: %d", edit_type);
                break;
            }
        }
    }

    Benchmark_End(benchmark, NULL);
}

static void M_TriggerTypeChange(
    const INJECTION *const injection, const SECTOR *const sector)
{
    const uint8_t new_type = VFile_ReadU8(injection->fp);
    if (sector == NULL || sector->trigger == NULL) {
        return;
    }

    sector->trigger->type = new_type;
}

static void M_TriggerParameterChange(
    const INJECTION *const injection, const SECTOR *const sector)
{
    const uint8_t cmd_type = VFile_ReadU8(injection->fp);
    const int16_t old_param = VFile_ReadS16(injection->fp);
    const int16_t new_param = VFile_ReadS16(injection->fp);

    if (sector == NULL || sector->trigger == NULL) {
        return;
    }

    // If we can find an action item for the given sector that matches
    // the command type and old (current) parameter, change it to the
    // new parameter.
    for (int32_t i = 0; i < sector->trigger->command_count; i++) {
        TRIGGER_CMD *const cmd = &sector->trigger->commands[i];
        if (cmd->type != cmd_type) {
            continue;
        }

        if (cmd->type == TO_CAMERA) {
            TRIGGER_CAMERA_DATA *const cam_data =
                (TRIGGER_CAMERA_DATA *)cmd->parameter;
            if (cam_data->camera_num == old_param) {
                cam_data->camera_num = new_param;
                break;
            }
        } else {
            if ((int16_t)(intptr_t)cmd->parameter == old_param) {
                cmd->parameter = (void *)(intptr_t)new_param;
                break;
            }
        }
    }
}

static void M_SetMusicOneShot(const SECTOR *const sector)
{
    if (sector == NULL || sector->trigger == NULL) {
        return;
    }

    for (int32_t i = 0; i < sector->trigger->command_count; i++) {
        const TRIGGER_CMD *const cmd = &sector->trigger->commands[i];
        if (cmd->type == TO_CD) {
            sector->trigger->one_shot = true;
            break;
        }
    }
}

static void M_InsertFloorData(
    const INJECTION *const injection, SECTOR *const sector)
{
    const int32_t data_length = VFile_ReadS32(injection->fp);
    int16_t data[data_length];
    VFile_Read(injection->fp, data, sizeof(int16_t) * data_length);

    if (sector == NULL) {
        return;
    }

    // This will reset all FD properties in the sector based on the raw data
    // imported. We pass a dummy null index to allow it to read from the
    // beginning of the array.
    Room_PopulateSectorData(sector, data, 0, NULL_FD_INDEX);
}

static void M_RoomShift(
    const INJECTION *const injection, const int16_t room_num)
{
    const uint32_t x_shift = ROUND_TO_SECTOR(VFile_ReadU32(injection->fp));
    const uint32_t z_shift = ROUND_TO_SECTOR(VFile_ReadU32(injection->fp));
    const int32_t y_shift = ROUND_TO_CLICK(VFile_ReadS32(injection->fp));

    ROOM *const room = Room_Get(room_num);
    room->pos.x += x_shift;
    room->pos.z += z_shift;
    room->min_floor += y_shift;
    room->max_ceiling += y_shift;

    // Move any items in the room to match.
    for (int32_t i = 0; i < Item_GetTotalCount(); i++) {
        ITEM *const item = Item_Get(i);
        if (item->room_num != room_num) {
            continue;
        }

        item->pos.x += x_shift;
        item->pos.y += y_shift;
        item->pos.z += z_shift;
    }

    if (y_shift == 0) {
        return;
    }

    // Update the sector floor and ceiling heights to match.
    for (int32_t i = 0; i < room->size.z * room->size.x; i++) {
        SECTOR *const sector = &room->sectors[i];
        if (sector->floor.height == NO_HEIGHT
            || sector->ceiling.height == NO_HEIGHT) {
            continue;
        }

        sector->floor.height += y_shift;
        sector->ceiling.height += y_shift;
    }

    // Update vertex Y values to match; x and z are room-relative.
#if TR_VERSION == 1
    for (int32_t i = 0; i < room->mesh.num_vertices; i++) {
        (&room->mesh.vertices[i])->pos.y += y_shift;
    }
#else
    int16_t *data_ptr = room->data;
    const int16_t vertex_count = *data_ptr++;
    for (int32_t i = 0; i < vertex_count; i++) {
        *(data_ptr + (i * 4) + 1) += y_shift;
    }
#endif
}

static void M_RoomProperties(
    const INJECTION *const injection, const int16_t room_num)
{
    const uint16_t flags = VFile_ReadU16(injection->fp);
    ROOM *const room = Room_Get(room_num);
    room->flags = flags;
}

static void M_TriggeredItem(const INJECTION *const injection)
{
#if TR_VERSION == 1
    /*if (Item_GetTotalCount() == MAX_ITEMS) {
        VFile_Skip(
            file, sizeof(int16_t) * 4 + sizeof(int32_t) * 3 + sizeof(uint16_t));
        LOG_WARNING("Cannot add more than %d items", MAX_ITEMS);
        return;
    }

    const int16_t item_num = Item_Create();
    ITEM *const item = Item_Get(item_num);

    item->object_id = VFile_ReadS16(injection->fp);
    item->room_num = VFile_ReadS16(injection->fp);
    item->pos.x = VFile_ReadS32(injection->fp);
    item->pos.y = VFile_ReadS32(injection->fp);
    item->pos.z = VFile_ReadS32(injection->fp);
    item->rot.y = VFile_ReadS16(injection->fp);
    item->shade = VFile_ReadS16(injection->fp);
    item->flags = VFile_ReadU16(injection->fp);

    LEVEL_INFO *const level_info = Level_GetInfo();
    level_info->item_count++;
    g_LevelItemCount++;*/
#else
    LOG_WARNING("Item injection is not currently supported");
#endif
}

static void M_ApplyItemEdits(
    const INJECTION_SET *const set, const int32_t data_count)
{
    BENCHMARK *const benchmark = Benchmark_Start();

    VFILE *const file = set->injection->fp;
    for (int32_t i = 0; i < data_count; i++) {
        const int16_t item_num = VFile_ReadS16(file);
        const int16_t y_rot = VFile_ReadS16(file);
        const XYZ_32 pos = {
            .x = VFile_ReadS32(file),
            .y = VFile_ReadS32(file),
            .z = VFile_ReadS32(file),
        };
        const int16_t room_num = VFile_ReadS16(file);

        if (item_num < 0 || item_num >= Item_GetTotalCount()) {
            LOG_WARNING("Item number %d is out of level item range", item_num);
            continue;
        }

        ITEM *const item = Item_Get(item_num);
        item->rot.y = y_rot;
        item->pos = pos;
        item->room_num = room_num;
    }

    Benchmark_End(benchmark, NULL);
}

static void M_ApplyObjectMeshEdits(
    const INJECTION_SET *const set, const int32_t data_count)
{
    BENCHMARK *const benchmark = Benchmark_Start();

    MESH_EDIT *mesh_edits = Memory_Alloc(sizeof(MESH_EDIT) * data_count);

    VFILE *const file = set->injection->fp;
    for (int32_t i = 0; i < data_count; i++) {
        MESH_EDIT *const mesh_edit = &mesh_edits[i];
        mesh_edit->object_id = VFile_ReadS32(file);
        mesh_edit->mesh_idx = VFile_ReadS16(file);
        mesh_edit->centre_shift.x = VFile_ReadS16(file);
        mesh_edit->centre_shift.y = VFile_ReadS16(file);
        mesh_edit->centre_shift.z = VFile_ReadS16(file);
        mesh_edit->radius_shift = VFile_ReadS32(file);

        mesh_edit->face_edit_count = VFile_ReadS32(file);
        mesh_edit->face_edits =
            Memory_Alloc(sizeof(FACE_EDIT) * mesh_edit->face_edit_count);
        for (int32_t j = 0; j < mesh_edit->face_edit_count; j++) {
            FACE_EDIT *const face_edit = &mesh_edit->face_edits[j];
            face_edit->object_id = VFile_ReadS32(file);
            face_edit->source_identifier = VFile_ReadS16(file);
            face_edit->face_type = VFile_ReadS32(file);
            face_edit->face_index = VFile_ReadS16(file);

            face_edit->target_count = VFile_ReadS32(file);
            face_edit->targets =
                Memory_Alloc(sizeof(int16_t) * face_edit->target_count);
            VFile_Read(
                file, face_edit->targets,
                sizeof(int16_t) * face_edit->target_count);
        }

        mesh_edit->vertex_edit_count = VFile_ReadS32(file);
        mesh_edit->vertex_edits =
            Memory_Alloc(sizeof(VERTEX_EDIT) * mesh_edit->vertex_edit_count);
        for (int32_t i = 0; i < mesh_edit->vertex_edit_count; i++) {
            VERTEX_EDIT *const vertex_edit = &mesh_edit->vertex_edits[i];
            vertex_edit->index = VFile_ReadS16(file);
            vertex_edit->shift.x = VFile_ReadS16(file);
            vertex_edit->shift.y = VFile_ReadS16(file);
            vertex_edit->shift.z = VFile_ReadS16(file);
        }

        M_ApplyMeshEdit(mesh_edit);

        for (int32_t j = 0; j < mesh_edit->face_edit_count; j++) {
            FACE_EDIT *face_edit = &mesh_edit->face_edits[j];
            Memory_FreePointer(&face_edit->targets);
        }

        Memory_FreePointer(&mesh_edit->face_edits);
        Memory_FreePointer(&mesh_edit->vertex_edits);
    }

    Memory_FreePointer(&mesh_edits);
    Benchmark_End(benchmark, NULL);
}

static void M_ApplyMeshEdit(const MESH_EDIT *const mesh_edit)
{
    const OBJECT *const object = Object_GetObject(mesh_edit->object_id);
    if (!object->loaded) {
        return;
    }

    OBJECT_MESH *const mesh =
        Object_GetMesh(object->mesh_idx + mesh_edit->mesh_idx);

    mesh->center.x += mesh_edit->centre_shift.x;
    mesh->center.y += mesh_edit->centre_shift.y;
    mesh->center.z += mesh_edit->centre_shift.z;
    mesh->radius += mesh_edit->radius_shift;

    for (int32_t i = 0; i < mesh_edit->vertex_edit_count; i++) {
        const VERTEX_EDIT *const edit = &mesh_edit->vertex_edits[i];
        XYZ_16 *const vertex = &mesh->vertices[edit->index];
        vertex->x += edit->shift.x;
        vertex->y += edit->shift.y;
        vertex->z += edit->shift.z;
    }

    // Find each face we are interested in and replace its texture
    // or palette reference with the one selected from each edit's
    // instructions.
    for (int32_t i = 0; i < mesh_edit->face_edit_count; i++) {
        const FACE_EDIT *const face_edit = &mesh_edit->face_edits[i];
        uint16_t texture;
        if (face_edit->source_identifier < 0) {
            const INJECT_INTERFACE *const interface = Inject_GetInterface();
            const uint16_t *palette_map = interface->get_pallete_map();
            texture = palette_map[-face_edit->source_identifier];
        } else {
            const uint16_t *const tex_ptr = M_GetMeshTexture(face_edit);
            if (tex_ptr == NULL) {
                continue;
            }
            texture = *tex_ptr;
        }

        switch (face_edit->face_type) {
        case FT_TEXTURED_QUAD:
            M_ApplyFace4Edit(face_edit, mesh->tex_face4s, texture);
            break;
        case FT_TEXTURED_TRIANGLE:
            M_ApplyFace3Edit(face_edit, mesh->tex_face3s, texture);
            break;
        case FT_COLOURED_QUAD:
            M_ApplyFace4Edit(face_edit, mesh->flat_face4s, texture);
            break;
        case FT_COLOURED_TRIANGLE:
            M_ApplyFace3Edit(face_edit, mesh->flat_face3s, texture);
            break;
        }
    }
}

static void M_ApplyFace4Edit(
    const FACE_EDIT *const edit, FACE4 *const faces, const uint16_t texture)
{
    for (int32_t i = 0; i < edit->target_count; i++) {
        FACE4 *const face = &faces[edit->targets[i]];
        face->texture = texture;
    }
}

static void M_ApplyFace3Edit(
    const FACE_EDIT *const edit, FACE3 *const faces, const uint16_t texture)
{
    for (int32_t i = 0; i < edit->target_count; i++) {
        FACE3 *const face = &faces[edit->targets[i]];
        face->texture = texture;
    }
}

static uint16_t *M_GetMeshTexture(const FACE_EDIT *const face_edit)
{
    const OBJECT *const object = Object_GetObject(face_edit->object_id);
    if (!object->loaded) {
        return NULL;
    }

    const OBJECT_MESH *const mesh =
        Object_GetMesh(object->mesh_idx + face_edit->source_identifier);

    if (face_edit->face_type == FT_TEXTURED_QUAD) {
        FACE4 *const face = &mesh->tex_face4s[face_edit->face_index];
        return &face->texture;
    }

    if (face_edit->face_type == FT_TEXTURED_TRIANGLE) {
        FACE3 *const face = &mesh->tex_face3s[face_edit->face_index];
        return &face->texture;
    }

    if (face_edit->face_type == FT_COLOURED_QUAD) {
        FACE4 *const face = &mesh->flat_face4s[face_edit->face_index];
        return &face->texture;
    }

    if (face_edit->face_type == FT_COLOURED_TRIANGLE) {
        FACE3 *const face = &mesh->flat_face3s[face_edit->face_index];
        return &face->texture;
    }

    return NULL;
}

static void M_ApplyRoomMeshEdits(
    const INJECTION_SET *const set, const int32_t data_count)
{
    BENCHMARK *const benchmark = Benchmark_Start();

    for (int32_t i = 0; i < data_count; i++) {
        const ROOM_MESH_EDIT_TYPE edit_type = VFile_ReadS32(set->injection->fp);

        switch (edit_type) {
        case RMET_TEXTURE_FACE:
            M_TextureRoomFace(set->injection);
            break;
        case RMET_MOVE_FACE:
            M_MoveRoomFace(set->injection);
            break;
        case RMET_ALTER_VERTEX:
            M_AlterRoomVertex(set->injection);
            break;
        case RMET_ROTATE_FACE:
            M_RotateRoomFace(set->injection);
            break;
        case RMET_ADD_FACE:
            M_AddRoomFace(set->injection);
            break;
        case RMET_ADD_VERTEX:
            M_AddRoomVertex(set->injection);
            break;
        case RMET_ADD_SPRITE:
            M_AddRoomSprite(set->injection);
            break;
        default:
            LOG_WARNING("Unknown room mesh edit type: %d", edit_type);
            break;
        }
    }

    Benchmark_End(benchmark, NULL);
}

static void M_TextureRoomFace(const INJECTION *const injection)
{
    const int16_t target_room = VFile_ReadS16(injection->fp);
    const FACE_TYPE target_face_type = VFile_ReadS32(injection->fp);
    const int16_t target_face = VFile_ReadS16(injection->fp);
    const int16_t source_room = VFile_ReadS16(injection->fp);
    const FACE_TYPE source_face_type = VFile_ReadS32(injection->fp);
    const int16_t source_face = VFile_ReadS16(injection->fp);
#if TR_VERSION == 1
    const uint16_t *const source_texture =
        M_GetRoomTexture(source_room, source_face_type, source_face);
    uint16_t *const target_texture =
        M_GetRoomTexture(target_room, target_face_type, target_face);
    if (source_texture != NULL && target_texture != NULL) {
        *target_texture = *source_texture;
    }
#endif
}

static void M_MoveRoomFace(const INJECTION *const injection)
{
    const int16_t target_room = VFile_ReadS16(injection->fp);
    const FACE_TYPE face_type = VFile_ReadS32(injection->fp);
    const int16_t target_face = VFile_ReadS16(injection->fp);
    const int32_t vertex_count = VFile_ReadS32(injection->fp);
#if TR_VERSION == 1
    for (int32_t j = 0; j < vertex_count; j++) {
        const int16_t vertex_index = VFile_ReadS16(injection->fp);
        const int16_t new_vertex = VFile_ReadS16(injection->fp);

        uint16_t *const vertices =
            M_GetRoomFaceVertices(target_room, face_type, target_face);
        if (vertices != NULL) {
            vertices[vertex_index] = new_vertex;
        }
    }
#endif
}

static void M_AlterRoomVertex(const INJECTION *const injection)
{
    const int16_t target_room = VFile_ReadS16(injection->fp);
    VFile_Skip(injection->fp, sizeof(int32_t));
    const int16_t target_vertex = VFile_ReadS16(injection->fp);
    const int16_t x_change = VFile_ReadS16(injection->fp);
    const int16_t y_change = VFile_ReadS16(injection->fp);
    const int16_t z_change = VFile_ReadS16(injection->fp);
    const int16_t shade_change = VFile_ReadS16(injection->fp);
#if TR_VERSION == 1
    if (target_room < 0 || target_room >= Room_GetTotalCount()) {
        LOG_WARNING("Room index %d is invalid", target_room);
        return;
    }

    const ROOM *const room = Room_Get(target_room);
    if (target_vertex < 0 || target_vertex >= room->mesh.num_vertices) {
        LOG_WARNING(
            "Vertex index %d, room %d is invalid", target_vertex, target_room);
        return;
    }

    ROOM_VERTEX *const vertex = &room->mesh.vertices[target_vertex];
    vertex->pos.x += x_change;
    vertex->pos.y += y_change;
    vertex->pos.z += z_change;
    vertex->shade += shade_change;
    CLAMPG(vertex->shade, MAX_LIGHTING);
#endif
}

static void M_RotateRoomFace(const INJECTION *const injection)
{
    const int16_t target_room = VFile_ReadS16(injection->fp);
    const FACE_TYPE face_type = VFile_ReadS32(injection->fp);
    const int16_t target_face = VFile_ReadS16(injection->fp);
    const uint8_t num_rotations = VFile_ReadU8(injection->fp);
#if TR_VERSION == 1
    uint16_t *const face_vertices =
        M_GetRoomFaceVertices(target_room, face_type, target_face);
    if (face_vertices == NULL) {
        return;
    }

    const int32_t num_vertices = face_type == FT_TEXTURED_QUAD ? 4 : 3;
    uint16_t *vertices[num_vertices];
    for (int32_t i = 0; i < num_vertices; i++) {
        vertices[i] = face_vertices + i;
    }

    for (int32_t i = 0; i < num_rotations; i++) {
        const uint16_t first = *vertices[0];
        for (int32_t j = 0; j < num_vertices - 1; j++) {
            *vertices[j] = *vertices[j + 1];
        }
        *vertices[num_vertices - 1] = first;
    }
#endif
}

static void M_AddRoomFace(const INJECTION *const injection)
{
    const int16_t target_room = VFile_ReadS16(injection->fp);
    const FACE_TYPE face_type = VFile_ReadS32(injection->fp);
    const int16_t source_room = VFile_ReadS16(injection->fp);
    const int16_t source_face = VFile_ReadS16(injection->fp);

    const int32_t num_vertices = face_type == FT_TEXTURED_QUAD ? 4 : 3;
    uint16_t vertices[num_vertices];
    for (int32_t i = 0; i < num_vertices; i++) {
        vertices[i] = VFile_ReadU16(injection->fp);
    }
#if TR_VERSION == 1
    if (target_room < 0 || target_room >= Room_GetTotalCount()) {
        LOG_WARNING("Room index %d is invalid", target_room);
        return;
    }

    const uint16_t *const source_texture =
        M_GetRoomTexture(source_room, face_type, source_face);
    if (source_texture == NULL) {
        return;
    }

    ROOM *const room = Room_Get(target_room);
    uint16_t *face_vertices;
    if (face_type == FT_TEXTURED_QUAD) {
        FACE4 *const face = &room->mesh.face4s[room->mesh.num_face4s];
        face->texture = *source_texture;
        face_vertices = face->vertices;
        room->mesh.num_face4s++;

    } else {
        FACE3 *const face = &room->mesh.face3s[room->mesh.num_face3s];
        face->texture = *source_texture;
        face_vertices = face->vertices;
        room->mesh.num_face3s++;
    }

    for (int32_t i = 0; i < num_vertices; i++) {
        face_vertices[i] = vertices[i];
    }
#endif
}

static void M_AddRoomVertex(const INJECTION *const injection)
{
    const int16_t target_room = VFile_ReadS16(injection->fp);
    VFile_Skip(injection->fp, sizeof(int32_t));
    const XYZ_16 pos = {
        .x = VFile_ReadS16(injection->fp),
        .y = VFile_ReadS16(injection->fp),
        .z = VFile_ReadS16(injection->fp),
    };
    const int16_t shade = VFile_ReadS16(injection->fp);
#if TR_VERSION == 1
    ROOM *const room = Room_Get(target_room);
    ROOM_VERTEX *const vertex = &room->mesh.vertices[room->mesh.num_vertices];
    vertex->pos = pos;
    vertex->shade = shade;
    room->mesh.num_vertices++;
#endif
}

static void M_AddRoomSprite(const INJECTION *const injection)
{
    const int16_t target_room = VFile_ReadS16(injection->fp);
    VFile_Skip(injection->fp, sizeof(int32_t));
    const uint16_t vertex = VFile_ReadU16(injection->fp);
    const uint16_t texture = VFile_ReadU16(injection->fp);
#if TR_VERSION == 1
    ROOM *const room = Room_Get(target_room);
    ROOM_SPRITE *const sprite = &room->mesh.sprites[room->mesh.num_sprites];
    sprite->vertex = vertex;
    sprite->texture = texture;
    room->mesh.num_sprites++;
#endif
}

static uint16_t *M_GetRoomTexture(
    const int16_t room_num, const FACE_TYPE face_type, const int16_t face_index)
{
#if TR_VERSION == 1
    const ROOM *const room = Room_Get(room_num);
    if (face_type == FT_TEXTURED_QUAD && face_index < room->mesh.num_face4s) {
        FACE4 *const face = &room->mesh.face4s[face_index];
        return &face->texture;
    } else if (face_index < room->mesh.num_face3s) {
        FACE3 *const face = &room->mesh.face3s[face_index];
        return &face->texture;
    }

    LOG_WARNING(
        "Invalid room face lookup: %d, %d, %d", room_num, face_type,
        face_index);
#endif
    return NULL;
}

static uint16_t *M_GetRoomFaceVertices(
    const int16_t room_num, const FACE_TYPE face_type, const int16_t face_index)
{
#if TR_VERSION == 1
    if (room_num < 0 || room_num >= Room_GetTotalCount()) {
        LOG_WARNING("Room index %d is invalid", room_num);
        return NULL;
    }

    const ROOM *const room = Room_Get(room_num);
    if (face_type == FT_TEXTURED_QUAD) {
        if (face_index < 0 || face_index >= room->mesh.num_face4s) {
            LOG_WARNING(
                "Face4 index %d, room %d is invalid", face_index, room_num);
            return NULL;
        }

        FACE4 *const face = &room->mesh.face4s[face_index];
        return (uint16_t *)(void *)&face->vertices;
    }

    if (face_type == FT_TEXTURED_TRIANGLE) {
        if (face_index < 0 || face_index >= room->mesh.num_face3s) {
            LOG_WARNING(
                "Face3 index %d, room %d is invalid", face_index, room_num);
            return NULL;
        }

        FACE3 *const face = &room->mesh.face3s[face_index];
        return (uint16_t *)(void *)&face->vertices;
    }
#endif
    return NULL;
}

static void M_ApplyPortalEdits(
    const INJECTION_SET *const set, const int32_t data_count)
{
    BENCHMARK *const benchmark = Benchmark_Start();

    VFILE *const file = set->injection->fp;
    for (int32_t i = 0; i < data_count; i++) {
        const int16_t base_room = VFile_ReadS16(file);
        const int16_t link_room = VFile_ReadS16(file);
        const int16_t door_index = VFile_ReadS16(file);

        if (base_room < 0 || base_room >= Room_GetTotalCount()) {
            VFile_Skip(file, sizeof(int16_t) * 12);
            LOG_WARNING("Room index %d is invalid", base_room);
            continue;
        }

        const ROOM *const room = Room_Get(base_room);
        PORTAL *portal = NULL;
        for (int32_t j = 0; j < room->portals->count; j++) {
            PORTAL *const test_portal = &room->portals->portal[j];
            if (test_portal->room_num == link_room && j == door_index) {
                portal = test_portal;
                break;
            }
        }

        if (portal == NULL) {
            VFile_Skip(file, sizeof(int16_t) * 12);
            LOG_WARNING(
                "Room index %d has no matching portal to %d", base_room,
                link_room);
            continue;
        }

        for (int32_t j = 0; j < 4; j++) {
            portal->vertex[j].x += VFile_ReadS16(file);
            portal->vertex[j].y += VFile_ReadS16(file);
            portal->vertex[j].z += VFile_ReadS16(file);
        }
    }

    Benchmark_End(benchmark, NULL);
}

bool Inject_InterpretSet(const INJECTION_SET *const set)
{
    const INJECT_INTERFACE *const interface = Inject_GetInterface();
    if (interface->handle_set != NULL && interface->handle_set(set)) {
        return true;
    }

    switch (set->type) {
    case IST_DATA_EDITS:
        M_InterpretSet(set);
        break;
    default:
        return false;
    }

    return true;
}
