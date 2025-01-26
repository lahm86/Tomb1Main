#include <libtrx/config.h>
#include <libtrx/game/inject.h>

bool Inject_IsRelevant(const INJECTION *const injection)
{
    switch (injection->type) {
    case IMT_GENERAL:
        return true;
    case IMT_FLOOR_DATA:
        return g_Config.gameplay.fix_floor_data_issues;
    case IMT_ITEM_POSITION:
        return g_Config.visuals.fix_item_rots;
    default:
        return false;
    }
}

bool Inject_HandleSet(const INJECTION_SET *const set)
{
    return false;
}
