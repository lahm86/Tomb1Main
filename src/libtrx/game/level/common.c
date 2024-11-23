#include "game/level/common.h"

#include "game/gamebuf.h"
#include "game/inject.h"
#include "game/objects/common.h"
#include "game/rooms.h"
#include "log.h"
#include "utils.h"
#include "vector.h"

static void M_ReadObjectMesh(OBJECT_MESH *mesh, const int16_t *data);

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

static void M_ReadObjectMesh(OBJECT_MESH *const mesh, const int16_t *data)
{
    mesh->center.x = *data++;
    mesh->center.y = *data++;
    mesh->center.z = *data++;
    mesh->radius = *data++;
    data++; // Originally 4 bytes for radius
    mesh->enable_reflection = false;

    {
        mesh->num_vertices = *data++;
        mesh->vertices =
            GameBuf_Alloc(sizeof(XYZ_16) * mesh->num_vertices, GBUF_MESHES);
        for (int32_t j = 0; j < mesh->num_vertices; j++) {
            XYZ_16 *const vertex = &mesh->vertices[j];
            vertex->x = *data++;
            vertex->y = *data++;
            vertex->z = *data++;
        }
    }

    {
        mesh->num_lights = *data++;
        if (mesh->num_lights > 0) {
            mesh->lighting.normals =
                GameBuf_Alloc(sizeof(XYZ_16) * mesh->num_lights, GBUF_MESHES);
            for (int32_t j = 0; j < mesh->num_lights; j++) {
                XYZ_16 *const normal = &mesh->lighting.normals[j];
                normal->x = *data++;
                normal->y = *data++;
                normal->z = *data++;
            }
        } else {
            mesh->lighting.lights = GameBuf_Alloc(
                sizeof(int16_t) * ABS(mesh->num_lights), GBUF_MESHES);
            for (int32_t j = 0; j < ABS(mesh->num_lights); j++) {
                mesh->lighting.lights[j] = *data++;
            }
        }
    }

    {
        mesh->num_tex_quads = *data++;
        mesh->tex_quads =
            GameBuf_Alloc(sizeof(FACE4) * mesh->num_tex_quads, GBUF_MESHES);
        for (int32_t j = 0; j < mesh->num_tex_quads; j++) {
            FACE4 *const face = &mesh->tex_quads[j];
            for (int k = 0; k < 4; k++) {
                face->vertices[k] = (uint16_t)*data++;
            }
            face->texture = (uint16_t)*data++;
            face->enable_reflection = false;
        }
    }

    {
        mesh->num_tex_triangles = *data++;
        mesh->tex_triangles =
            GameBuf_Alloc(sizeof(FACE3) * mesh->num_tex_triangles, GBUF_MESHES);
        for (int32_t j = 0; j < mesh->num_tex_triangles; j++) {
            FACE3 *const face = &mesh->tex_triangles[j];
            for (int k = 0; k < 3; k++) {
                face->vertices[k] = (uint16_t)*data++;
            }
            face->texture = (uint16_t)*data++;
            face->enable_reflection = false;
        }
    }

    {
        mesh->num_flat_quads = *data++;
        mesh->flat_quads =
            GameBuf_Alloc(sizeof(FACE4) * mesh->num_flat_quads, GBUF_MESHES);
        for (int32_t j = 0; j < mesh->num_flat_quads; j++) {
            FACE4 *const face = &mesh->flat_quads[j];
            for (int k = 0; k < 4; k++) {
                face->vertices[k] = (uint16_t)*data++;
            }
            face->texture = (uint16_t)*data++;
            face->enable_reflection = false;
        }
    }

    {
        mesh->num_flat_triangles = *data++;
        mesh->flat_triangles = GameBuf_Alloc(
            sizeof(FACE3) * mesh->num_flat_triangles, GBUF_MESHES);
        for (int32_t j = 0; j < mesh->num_flat_triangles; j++) {
            FACE3 *const face = &mesh->flat_triangles[j];
            for (int k = 0; k < 3; k++) {
                face->vertices[k] = (uint16_t)*data++;
            }
            face->texture = (uint16_t)*data++;
            face->enable_reflection = false;
        }
    }
}

void Level_ReadObjectMeshes(
    const int32_t base_mesh_count, const int16_t *const mesh_data,
    const int32_t *const mesh_indices)
{
    // We only construct and store distinct meshes e.g. Lara's hips are
    // referenced by several pointers as a dummy mesh.
    VECTOR *const unique_ptrs =
        Vector_CreateAtCapacity(sizeof(int32_t), base_mesh_count);
    int32_t ptr_map[base_mesh_count];
    for (int32_t i = 0; i < base_mesh_count; i++) {
        const int32_t ptr = mesh_indices[i];
        const int32_t index = Vector_IndexOf(unique_ptrs, (void *)&ptr);
        if (index == -1) {
            ptr_map[i] = unique_ptrs->count;
            Vector_Add(unique_ptrs, (void *)&ptr);
        } else {
            ptr_map[i] = index;
        }
    }

    OBJECT_MESH *const meshes =
        GameBuf_Alloc(sizeof(OBJECT_MESH) * unique_ptrs->count, GBUF_MESHES);
    for (int32_t i = 0; i < unique_ptrs->count; i++) {
        const int32_t ptr = *(const int32_t *)Vector_Get(unique_ptrs, i);
        M_ReadObjectMesh(&meshes[i], &mesh_data[ptr / 2]);
    }

    OBJECT_MESH *ptrs[base_mesh_count];
    for (int32_t i = 0; i < base_mesh_count; i++) {
        ptrs[i] = &meshes[ptr_map[i]];
    }

    Object_StoreMeshes(ptrs, base_mesh_count);
    Vector_Free(unique_ptrs);
}
