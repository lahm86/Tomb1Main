#include "game/game_flow/common.h"

#include "game/game_flow/vars.h"
#include "global/vars.h"

#include <libtrx/memory.h>

const char *GF_GetTitleLevelPath(void)
{
    if (g_GameFlow.title_level == NULL) {
        return NULL;
    }
    return g_GameFlow.title_level->path;
}

int32_t GF_GetLevelCount(void)
{
    return g_GameFlow.level_count;
}

int32_t GF_GetDemoCount(void)
{
    return g_GameFlow.demo_level_count;
}

const char *GF_GetLevelPath(const int32_t level_num)
{
    return g_GameFlow.levels[level_num].path;
}

const char *GF_GetLevelTitle(const int32_t level_num)
{
    return g_GameFlow.levels[level_num].title;
}

int32_t GF_GetCutsceneCount(void)
{
    return g_GameFlow.cutscene_count;
}

const char *GF_GetCutscenePath(const int32_t cutscene_num)
{
    return g_GameFlow.cutscenes[cutscene_num].path;
}

void GF_SetLevelTitle(const int32_t level_num, const char *const title)
{
    Memory_FreePointer(&g_GameFlow.levels[level_num].title);
    g_GameFlow.levels[level_num].title =
        title != NULL ? Memory_DupStr(title) : NULL;
}

int32_t GF_GetGymLevelNum(void)
{
    return g_GameFlow.gym_enabled ? LV_GYM : -1;
}

void GF_OverrideCommand(const GAME_FLOW_COMMAND command)
{
    g_GF_OverrideCommand = command;
}

GAME_FLOW_COMMAND GF_GetOverrideCommand(void)
{
    return g_GF_OverrideCommand;
}
