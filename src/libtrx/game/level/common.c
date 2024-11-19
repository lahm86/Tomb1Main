#include "game/level/common.h"

#include "game/gamebuf.h"
#include "game/inject.h"
#include "game/rooms.h"
#include "log.h"

void Level_ReadRoomMesh(int32_t room_num, VFILE *file)
{
    const uint32_t mesh_length = VFile_ReadU32(file) * sizeof(int16_t);
    size_t start_pos = VFile_GetPos(file);

    ROOM *const room = Room_Get(room_num);
    const INJ_MESH_META inj_data = Inject_GetRoomMeshMeta(room_num);

    {
        room->mesh.num_vertices = VFile_ReadS16(file);
        const int32_t alloc_count =
            room->mesh.num_vertices + inj_data.num_vertices;
        room->mesh.vertices =
            GameBuf_Alloc(sizeof(ROOM_VERTEX) * alloc_count, GBUF_ROOM_MESH);
        for (int32_t i = 0; i < room->mesh.num_vertices; i++) {
            ROOM_VERTEX *const vertex = &room->mesh.vertices[i];
            vertex->pos.x = VFile_ReadS16(file);
            vertex->pos.y = VFile_ReadS16(file);
            vertex->pos.z = VFile_ReadS16(file);
            vertex->shade = VFile_ReadU16(file);
            vertex->flags = 0;
        }
    }

    {
        room->mesh.num_quads = VFile_ReadS16(file);
        const int32_t alloc_count = room->mesh.num_quads + inj_data.num_quads;
        room->mesh.quads =
            GameBuf_Alloc(sizeof(FACE4) * alloc_count, GBUF_ROOM_MESH);
        for (int32_t i = 0; i < room->mesh.num_quads; i++) {
            FACE4 *const face = &room->mesh.quads[i];
            for (int32_t j = 0; j < 4; j++) {
                face->vertices[j] = VFile_ReadU16(file);
            }
            face->texture = VFile_ReadU16(file);
        }
    }

    {
        room->mesh.num_triangles = VFile_ReadS16(file);
        const int32_t alloc_count =
            room->mesh.num_triangles + inj_data.num_triangles;
        room->mesh.triangles =
            GameBuf_Alloc(sizeof(FACE4) * alloc_count, GBUF_ROOM_MESH);
        for (int32_t i = 0; i < room->mesh.num_triangles; i++) {
            FACE3 *const face = &room->mesh.triangles[i];
            for (int32_t j = 0; j < 3; j++) {
                face->vertices[j] = VFile_ReadU16(file);
            }
            face->texture = VFile_ReadU16(file);
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

    const size_t end_pos = VFile_GetPos(file);
    if (end_pos > start_pos + mesh_length) {
        LOG_ERROR(
            "Room %d mesh is too long: expected %d, read %d", room_num,
            mesh_length, end_pos - start_pos);
    }
}
