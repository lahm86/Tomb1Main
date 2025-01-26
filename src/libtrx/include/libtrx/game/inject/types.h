#pragma once

#include "../../virtual_file.h"
#include "../math.h"
#include "../objects/ids.h"
#include "./enum.h"

#include <stdint.h>

typedef struct {
    VFILE *fp;
    INJECTION_VERSION version;
    INJECTION_MAIN_TYPE type;
    bool relevant;
} INJECTION;

typedef struct {
    INJECTION *injection;
    INJECTION_SET_TYPE type;
    int32_t num_blocks;
} INJECTION_SET;

typedef struct {
    int16_t room_index;
    int16_t num_vertices;
    int16_t num_quads;
    int16_t num_triangles;
    int16_t num_sprites;
} INJECTION_MESH_META;

typedef struct {
    GAME_OBJECT_ID object_id;
    int16_t source_identifier;
    FACE_TYPE face_type;
    int16_t face_index;
    int32_t target_count;
    int16_t *targets;
} FACE_EDIT;

typedef struct {
    int16_t index;
    XYZ_16 shift;
} VERTEX_EDIT;

typedef struct {
    GAME_OBJECT_ID object_id;
    int16_t mesh_idx;
    XYZ_16 centre_shift;
    int32_t radius_shift;
    int32_t face_edit_count;
    int32_t vertex_edit_count;
    FACE_EDIT *face_edits;
    VERTEX_EDIT *vertex_edits;
} MESH_EDIT;
