#pragma once

#include "./types.h"

bool GF_IsGymEnabled(void);

extern int32_t GF_GetLevelCount(void);
extern int32_t GF_GetDemoCount(void);
extern const char *GF_GetLevelPath(int32_t level_num);
extern const char *GF_GetLevelTitle(int32_t level_num);
extern void GF_SetLevelTitle(int32_t level_num, const char *title);
extern int32_t GF_GetGymLevelNum(void);

extern void GF_OverrideCommand(GAME_FLOW_COMMAND action);
extern GAME_FLOW_COMMAND GF_GetOverrideCommand(void);
