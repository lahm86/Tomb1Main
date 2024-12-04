#pragma once

#include "./enums.h"
#include "./types.h"

#define BONE_SIZE 4

void Anim_InitialiseAnims(int32_t num_anims);
void Anim_InitialiseChanges(int32_t num_changes);
void Anim_InitialiseRanges(int32_t num_ranges);
void Anim_InitialiseCommands(int32_t num_commands);
void Anim_InitialiseBones(int32_t num_bones);
void Anim_InitialiseFrames(int32_t num_frames);

ANIM *Anim_GetAnim(int32_t anim_idx);
ANIM_CHANGE *Anim_GetChange(int32_t change_idx);
ANIM_RANGE *Anim_GetRange(int32_t range_idx);
int16_t *Anim_GetCommand(int32_t command_idx);
ANIM_BONE *Anim_GetBone(int32_t bone_idx);
FRAME_INFO *Anim_GetFrame(int32_t frame_idx);

void Anim_AddBoneFlags(int32_t bone_idx, BONE_FLAGS flags);
