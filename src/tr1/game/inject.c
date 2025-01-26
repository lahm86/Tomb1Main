#include <libtrx/config.h>
#include <libtrx/game/inject.h>

bool Inject_IsRelevant(const INJECTION *const injection)
{
    switch (injection->type) {
    case IMT_GENERAL:
    case IMT_LARA_ANIMS:
        return true;
    case IMT_BRAID:
        return g_Config.visuals.enable_braid;
    case IMT_UZI_SFX:
        return g_Config.audio.enable_ps_uzi_sfx;
    case IMT_FLOOR_DATA:
        return g_Config.gameplay.fix_floor_data_issues;
    case IMT_TEXTURE_FIX:
        return g_Config.visuals.fix_texture_issues;
    case IMT_ITEM_POSITION:
        return g_Config.visuals.fix_item_rots;
    case IMT_PS1_ENEMY:
        return g_Config.gameplay.restore_ps1_enemies;
    case IMT_DISABLE_ANIM_SPRITE:
        return !g_Config.visuals.fix_animated_sprites;
    case IMT_SKYBOX:
        return g_Config.visuals.enable_skybox;
    case IMT_PS1_CRYSTAL:
        return g_Config.gameplay.enable_save_crystals
            && g_Config.visuals.enable_ps1_crystals;
    default:
        return false;
    }
}

bool Inject_HandleSet(const INJECTION_SET *const set)
{
    return false;
}
