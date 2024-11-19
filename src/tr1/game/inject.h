#pragma once

#include "global/types.h"

#include <stdint.h>

typedef struct {
    int16_t room_index;
    int16_t num_vertices;
    int16_t num_quads;
    int16_t num_triangles;
    int16_t num_sprites;
} INJ_MESH_META;

typedef struct {
    int32_t texture_page_count;
    int32_t texture_count;
    int32_t sprite_info_count;
    int32_t sprite_count;
    int32_t mesh_count;
    int32_t mesh_ptr_count;
    int32_t anim_change_count;
    int32_t anim_range_count;
    int32_t anim_cmd_count;
    int32_t anim_bone_count;
    int32_t anim_frame_data_count;
    int32_t anim_frame_count;
    int32_t anim_frame_mesh_rot_count;
    int32_t anim_count;
    int32_t object_count;
    int32_t sfx_count;
    int32_t sfx_data_size;
    int32_t sample_count;
    int32_t mesh_edit_count;
    int32_t texture_overwrite_count;
    int32_t floor_edit_count;
    int32_t room_mesh_meta_count;
    INJ_MESH_META *room_mesh_meta;
    int32_t room_mesh_edit_count;
    int32_t room_door_edit_count;
    int32_t anim_range_edit_count;
    int32_t item_position_count;
} INJECTION_INFO;

void Inject_Init(
    int injection_count, char *filenames[], INJECTION_INFO *aggregate);
void Inject_AllInjections(LEVEL_INFO *level_info);
void Inject_Cleanup(void);
INJ_MESH_META Inject_GetRoomMeshMeta(int32_t room_index);
