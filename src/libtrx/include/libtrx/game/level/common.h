#pragma once

#include "../../virtual_file.h"
#include "../objects/types.h"

void Level_ReadRoomMesh(int32_t room_num, VFILE *file);
void Level_ReadObjectMeshes(
    int32_t base_mesh_count, const int16_t *mesh_data,
    const int32_t *mesh_indices);
