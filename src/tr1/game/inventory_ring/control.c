#include "game/inventory_ring/control.h"

#include "game/camera.h"
#include "game/console/common.h"
#include "game/game.h"
#include "game/game_flow.h"
#include "game/game_string.h"
#include "game/input.h"
#include "game/interpolation.h"
#include "game/inventory.h"
#include "game/inventory_ring/vars.h"
#include "game/lara/common.h"
#include "game/music.h"
#include "game/option.h"
#include "game/option/option_compass.h"
#include "game/option/option_examine.h"
#include "game/output.h"
#include "game/overlay.h"
#include "game/savegame.h"
#include "game/shell.h"
#include "game/sound.h"
#include "game/stats.h"
#include "game/viewport.h"
#include "global/vars.h"

#include <libtrx/config.h>
#include <libtrx/game/inventory_ring/priv.h>
#include <libtrx/memory.h>

#define INV_RING_FADE_TIME_FAST                                                \
    (INV_RING_CLOSE_FRAMES / INV_RING_FRAMES / (double)LOGIC_FPS)
#define INV_RING_FADE_TIME_TITLE_FINISH 0.25

static TEXTSTRING *m_ExamineItemText = nullptr;
static TEXTSTRING *m_UseItemText = nullptr;
static CLOCK_TIMER m_DemoTimer = { .type = CLOCK_TIMER_SIM };
static int32_t m_StartLevel;
static GAME_OBJECT_ID m_InvChosen;

static TEXTSTRING *M_InitExamineText(
    int32_t x_pos, const char *role_str, const char *input_str);
static void M_InitExamineOverlay(INV_RING *ring);
static void M_RemoveExamineOverlay(void);
static void M_ShowAmmoQuantity(const char *fmt, int32_t qty);

static void M_RingIsOpen(INV_RING *ring);
static void M_RingIsNotOpen(INV_RING *ring);
static void M_RingNotActive(const INVENTORY_ITEM *inv_item);
static void M_RingActive(INV_RING *ring);

static bool M_AnimateInventoryItem(INVENTORY_ITEM *inv_item);

static GF_COMMAND M_Finish(INV_RING *ring, bool apply_changes);
static GF_COMMAND M_Control(INV_RING *ring);
static bool M_CheckDemoTimer(const INV_RING *ring);

static TEXTSTRING *M_InitExamineText(
    const int32_t x_pos, const char *const role_str,
    const char *const input_str)
{
    char role[100];
    sprintf(role, role_str, input_str);

    TEXTSTRING *const text = Text_Create(x_pos, -100, role);
    Text_AlignBottom(text, true);
    Text_CentreH(text, true);
    Text_Hide(text, true);
    return text;
}

static void M_InitExamineOverlay(INV_RING *const ring)
{
    if ((ring->mode != INV_GAME_MODE && ring->mode != INV_KEYS_MODE)
        || !g_Config.gameplay.enable_item_examining
        || m_ExamineItemText != nullptr) {
        return;
    }

    m_ExamineItemText =
        M_InitExamineText(-100, GS(ITEM_EXAMINE_ROLE), GS(KEYMAP_LOOK));
    m_UseItemText =
        M_InitExamineText(100, GS(ITEM_USE_ROLE), GS(KEYMAP_ACTION));
}

static void M_RemoveExamineOverlay(void)
{
    if (m_ExamineItemText == nullptr) {
        return;
    }

    Text_Remove(m_ExamineItemText);
    Text_Remove(m_UseItemText);
    m_ExamineItemText = nullptr;
    m_UseItemText = nullptr;
}

static void M_ShowAmmoQuantity(const char *const fmt, const int32_t qty)
{
    if (!(g_GameInfo.bonus_flag & GBF_NGPLUS)) {
        InvRing_ShowItemQuantity(fmt, qty);
    }
}

static void M_RingIsOpen(INV_RING *const ring)
{
    InvRing_ShowHeader(ring);
    M_InitExamineOverlay(ring);
}

static void M_RingIsNotOpen(INV_RING *const ring)
{
    InvRing_RemoveHeader();
    M_RemoveExamineOverlay();
}

static void M_RingNotActive(const INVENTORY_ITEM *const inv_item)
{
    InvRing_ShowItemName(inv_item);

    const int32_t qty = Inv_RequestItem(inv_item->object_id);
    bool show_examine_option = false;

    switch (inv_item->object_id) {
    case O_SHOTGUN_OPTION:
        M_ShowAmmoQuantity("%5d A", g_Lara.shotgun.ammo / SHOTGUN_AMMO_CLIP);
        break;

    case O_MAGNUM_OPTION:
        M_ShowAmmoQuantity("%5d B", g_Lara.magnums.ammo);
        break;

    case O_UZI_OPTION:
        M_ShowAmmoQuantity("%5d C", g_Lara.uzis.ammo);
        break;

    case O_SG_AMMO_OPTION:
        InvRing_ShowItemQuantity("%d", qty * NUM_SG_SHELLS);
        break;

    case O_MAG_AMMO_OPTION:
    case O_UZI_AMMO_OPTION:
        InvRing_ShowItemQuantity("%d", qty * 2);
        break;

    case O_MEDI_OPTION:
    case O_BIGMEDI_OPTION:
        Overlay_BarSetHealthTimer(40);
        if (qty > 1) {
            InvRing_ShowItemQuantity("%d", qty);
        }
        break;

    case O_KEY_OPTION_1:
    case O_KEY_OPTION_2:
    case O_KEY_OPTION_3:
    case O_KEY_OPTION_4:
    case O_LEADBAR_OPTION:
    case O_PICKUP_OPTION_1:
    case O_PICKUP_OPTION_2:
    case O_PUZZLE_OPTION_1:
    case O_PUZZLE_OPTION_2:
    case O_PUZZLE_OPTION_3:
    case O_PUZZLE_OPTION_4:
    case O_SCION_OPTION:
        if (qty > 1) {
            InvRing_ShowItemQuantity("%d", qty);
        }

        show_examine_option = !Option_Examine_IsActive()
            && Option_Examine_CanExamine(inv_item->object_id);
        break;

    default:
        break;
    }

    if (inv_item->object_id == O_MEDI_OPTION
        || inv_item->object_id == O_BIGMEDI_OPTION) {
        if (g_Config.ui.healthbar_location == BL_TOP_LEFT) {
            InvRing_HideArrow(INV_RING_ARROW_TL, true);
        } else if (g_Config.ui.healthbar_location == BL_TOP_RIGHT) {
            InvRing_HideArrow(INV_RING_ARROW_TR, true);
        } else if (g_Config.ui.healthbar_location == BL_BOTTOM_LEFT) {
            InvRing_HideArrow(INV_RING_ARROW_BL, true);
        } else if (g_Config.ui.healthbar_location == BL_BOTTOM_RIGHT) {
            InvRing_HideArrow(INV_RING_ARROW_BR, true);
        }
        g_GameInfo.inv_showing_medpack = true;
    } else {
        InvRing_HideArrow(INV_RING_ARROW_TL, false);
        InvRing_HideArrow(INV_RING_ARROW_TR, false);
        InvRing_HideArrow(INV_RING_ARROW_BL, false);
        InvRing_HideArrow(INV_RING_ARROW_BR, false);
        g_GameInfo.inv_showing_medpack = false;
    }

    if (m_ExamineItemText != nullptr) {
        Text_Hide(m_ExamineItemText, !show_examine_option);
        Text_Hide(m_UseItemText, !show_examine_option);
    }
}

static void M_RingActive(INV_RING *const ring)
{
    InvRing_RemoveItemTexts();
}

static bool M_AnimateInventoryItem(INVENTORY_ITEM *const inv_item)
{
    if (inv_item->current_frame == inv_item->goal_frame) {
        InvRing_SelectMeshes(inv_item);
        return false;
    }

    if (inv_item->anim_count > 0) {
        inv_item->anim_count--;
    } else {
        inv_item->anim_count = inv_item->anim_speed;
        inv_item->current_frame += inv_item->anim_direction;
        if (inv_item->current_frame >= inv_item->frames_total) {
            inv_item->current_frame = 0;
        } else if (inv_item->current_frame < 0) {
            inv_item->current_frame = inv_item->frames_total - 1;
        }
    }

    InvRing_SelectMeshes(inv_item);
    return true;
}

static GF_COMMAND M_Finish(INV_RING *const ring, const bool apply_changes)
{
    // TODO: Make this function not have any side effects.
    // Consider adding new GF_ constants, but research other solutions first.

    if (m_StartLevel != -1) {
        return (GF_COMMAND) {
            .action = GF_SELECT_GAME,
            .param = m_StartLevel,
        };
    }

    if (ring->is_demo_needed) {
        return (GF_COMMAND) { .action = GF_START_DEMO, .param = -1 };
    }

    switch (m_InvChosen) {
    case O_PASSPORT_OPTION:
        switch (g_GameInfo.passport_selection) {
        case PASSPORT_MODE_LOAD_GAME:
            return (GF_COMMAND) {
                .action = GF_START_SAVED_GAME,
                .param = g_GameInfo.select_save_slot,
            };

        case PASSPORT_MODE_SELECT_LEVEL:
            return (GF_COMMAND) {
                .action = GF_SELECT_GAME,
                .param = g_GameInfo.select_level_num,
            };

        case PASSPORT_MODE_STORY_SO_FAR:
            return (GF_COMMAND) {
                .action = GF_STORY_SO_FAR,
                .param = g_GameInfo.select_save_slot,
            };

        case PASSPORT_MODE_NEW_GAME:
            if (apply_changes) {
                Savegame_InitCurrentInfo();
                Savegame_UnbindSlot();
            }
            return (GF_COMMAND) {
                .action = GF_START_GAME,
                .param = GF_GetFirstLevel()->num,
            };

        case PASSPORT_MODE_SAVE_GAME:
            if (apply_changes) {
                Savegame_Save(g_GameInfo.select_save_slot);
            }
            return (GF_COMMAND) { .action = GF_NOOP };

        case PASSPORT_MODE_RESTART:
            return (GF_COMMAND) {
                .action = GF_RESTART_GAME,
                .param = Game_GetCurrentLevel()->num,
            };

        case PASSPORT_MODE_EXIT_TITLE:
            return (GF_COMMAND) { .action = GF_EXIT_TO_TITLE };

        case PASSPORT_MODE_EXIT_GAME:
            return (GF_COMMAND) { .action = GF_EXIT_GAME };

        case PASSPORT_MODE_BROWSE:
        case PASSPORT_MODE_UNAVAILABLE:
        default:
            return (GF_COMMAND) { .action = GF_EXIT_TO_TITLE };
        }

    case O_PHOTO_OPTION:
        if (apply_changes) {
            Savegame_UnbindSlot();
        }
        if (GF_GetGymLevel() != nullptr) {
            return (GF_COMMAND) {
                .action = GF_START_GAME,
                .param = GF_GetGymLevel()->num,
            };
        }
        break;

    case O_PISTOL_OPTION:
    case O_SHOTGUN_OPTION:
    case O_MAGNUM_OPTION:
    case O_UZI_OPTION:
    case O_MEDI_OPTION:
    case O_BIGMEDI_OPTION:
    case O_KEY_OPTION_1:
    case O_KEY_OPTION_2:
    case O_KEY_OPTION_3:
    case O_KEY_OPTION_4:
    case O_PUZZLE_OPTION_1:
    case O_PUZZLE_OPTION_2:
    case O_PUZZLE_OPTION_3:
    case O_PUZZLE_OPTION_4:
    case O_LEADBAR_OPTION:
    case O_SCION_OPTION:
        if (apply_changes) {
            Lara_UseItem(m_InvChosen);
        }
        break;

    default:
        break;
    }

    return (GF_COMMAND) { .action = GF_NOOP };
}

static GF_COMMAND M_Control(INV_RING *const ring)
{
    if (ring->motion.status == RNG_OPENING) {
        if (ring->mode == INV_TITLE_MODE
            && (Fader_IsActive(&ring->top_fader)
                || Fader_IsActive(&ring->back_fader))) {
            return (GF_COMMAND) { .action = GF_NOOP };
        }

        ClockTimer_Sync(&m_DemoTimer);
        if (!ring->has_spun_out) {
            Sound_Effect(SFX_MENU_SPININ, nullptr, SPM_ALWAYS);
            ring->has_spun_out = true;
        }
    }

    if (ring->motion.status == RNG_FADING_OUT) {
        if (!Fader_IsActive(&ring->back_fader)
            && !Fader_IsActive(&ring->top_fader)) {
            Fader_InitEx(
                &ring->top_fader,
                (FADER_ARGS) {
                    .initial = FADER_ANY,
                    .target = FADER_BLACK,
                    .duration = INV_RING_FADE_TIME_TITLE_FINISH,
                    .debuff = 1. / (double)LOGIC_FPS,
                });
        }

        if (Fader_IsActive(&ring->top_fader)
            || Fader_IsActive(&ring->back_fader)) {
            return (GF_COMMAND) { .action = GF_NOOP };
        }
        ring->motion.status = RNG_DONE;
    }

    if (ring->motion.status == RNG_DONE) {
        const GF_COMMAND gf_cmd = M_Finish(ring, true);
        // Returning to game – resume music
        if (gf_cmd.action == GF_NOOP) {
            Music_Unpause();
            Sound_UnpauseAll();
        }
        return gf_cmd;
    }

    InvRing_CalcAdders(ring, INV_RING_ROTATE_DURATION);

    Input_Update();
    // Do the demo inactivity check prior to postprocessing of the inputs.
    if (M_CheckDemoTimer(ring)) {
        ring->is_demo_needed = true;
    }
    Shell_ProcessInput();
    Game_ProcessInput();

    m_StartLevel = g_LevelComplete ? g_GameInfo.select_level_num : -1;

    g_GameInfo.inv_ring_above = ring->mode == INV_GAME_MODE
        && ((ring->type == RT_MAIN && g_InvRing_Source[RT_KEYS].count != 0)
            || (ring->type == RT_OPTION && g_InvRing_Source[RT_MAIN].count));

    if (g_Config.gameplay.enable_timer_in_inventory) {
        Stats_UpdateTimer();
    }

    if (ring->rotating) {
        return (GF_COMMAND) { .action = GF_NOOP };
    }

    if ((ring->mode == INV_SAVE_MODE || ring->mode == INV_SAVE_CRYSTAL_MODE
         || ring->mode == INV_LOAD_MODE || ring->mode == INV_DEATH_MODE)
        && !ring->is_pass_open) {
        g_Input = (INPUT_STATE) {};
        g_InputDB = (INPUT_STATE) { .menu_confirm = 1 };
    }

    if (ring->mode != INV_TITLE_MODE && !Fader_IsActive(&ring->back_fader)
        && !Fader_IsActive(&ring->top_fader)
        && ring->motion.status != RNG_OPENING) {
        for (int i = 0; i < ring->number_of_objects; i++) {
            INVENTORY_ITEM *const inv_item = ring->list[i];
            if (inv_item->object_id == O_COMPASS_OPTION) {
                Option_Compass_UpdateNeedle(inv_item);
            }
        }
    }

    switch (ring->motion.status) {
    case RNG_OPEN:
        if (g_Input.menu_right && ring->number_of_objects > 1) {
            InvRing_RotateLeft(ring);
            Sound_Effect(SFX_MENU_ROTATE, nullptr, SPM_ALWAYS);
            break;
        }

        if (g_Input.menu_left && ring->number_of_objects > 1) {
            InvRing_RotateRight(ring);
            Sound_Effect(SFX_MENU_ROTATE, nullptr, SPM_ALWAYS);
            break;
        }

        if (m_StartLevel != -1 || ring->is_demo_needed
            || (g_InputDB.menu_back && ring->mode != INV_TITLE_MODE)) {
            Sound_Effect(SFX_MENU_SPINOUT, nullptr, SPM_ALWAYS);
            m_InvChosen = NO_OBJECT;

            if (ring->type == RT_MAIN) {
                g_InvRing_Source[RT_MAIN].current = ring->current_object;
            } else {
                g_InvRing_Source[RT_OPTION].current = ring->current_object;
            }

            if (M_Finish(ring, false).action != GF_NOOP) {
                InvRing_MotionSetup(
                    ring, RNG_CLOSING, RNG_FADING_OUT, INV_RING_CLOSE_FRAMES);
            } else {
                InvRing_MotionSetup(
                    ring, RNG_CLOSING, RNG_DONE, INV_RING_CLOSE_FRAMES);
                Fader_Init(
                    &ring->back_fader, FADER_ANY, FADER_TRANSPARENT,
                    INV_RING_FADE_TIME_FAST);
            }
            InvRing_MotionRadius(ring, 0);
            InvRing_MotionCameraPos(ring, INV_RING_CAMERA_START_HEIGHT);
            InvRing_MotionRotation(
                ring, INV_RING_CLOSE_ROTATION,
                ring->ring_pos.rot.y - INV_RING_CLOSE_ROTATION);

            g_Input = (INPUT_STATE) {};
            g_InputDB = (INPUT_STATE) {};
        }

        const bool examine = g_InputDB.look && InvRing_CanExamine();
        if (g_InputDB.menu_confirm || examine) {
            if ((ring->mode == INV_SAVE_MODE
                 || ring->mode == INV_SAVE_CRYSTAL_MODE
                 || ring->mode == INV_LOAD_MODE || ring->mode == INV_DEATH_MODE)
                && !ring->is_pass_open) {
                ring->is_pass_open = true;
            }

            g_OptionSelected = 0;

            g_InvRing_Source[ring->type].current = ring->current_object;
            INVENTORY_ITEM *const inv_item =
                g_InvRing_Source[ring->type].items[ring->current_object];

            inv_item->goal_frame = inv_item->open_frame;
            inv_item->anim_direction = 1;
            inv_item->action = examine ? ACTION_EXAMINE : ACTION_USE;

            InvRing_MotionSetup(
                ring, RNG_SELECTING, RNG_SELECTED, SELECTING_FRAMES);
            InvRing_MotionRotation(
                ring, 0, -DEG_90 - ring->angle_adder * ring->current_object);
            InvRing_MotionItemSelect(ring, inv_item);
            g_Input = (INPUT_STATE) {};
            g_InputDB = (INPUT_STATE) {};

            switch (inv_item->object_id) {
            case O_COMPASS_OPTION:
                Sound_Effect(SFX_MENU_COMPASS, nullptr, SPM_ALWAYS);
                break;

            case O_PHOTO_OPTION:
                Sound_Effect(SFX_MENU_LARA_HOME, nullptr, SPM_ALWAYS);
                break;

            case O_CONTROL_OPTION:
                Sound_Effect(SFX_MENU_GAMEBOY, nullptr, SPM_ALWAYS);
                break;

            case O_PISTOL_OPTION:
            case O_SHOTGUN_OPTION:
            case O_MAGNUM_OPTION:
            case O_UZI_OPTION:
                Sound_Effect(SFX_MENU_GUNS, nullptr, SPM_ALWAYS);
                break;

            default:
                Sound_Effect(SFX_MENU_SPININ, nullptr, SPM_ALWAYS);
                break;
            }
        }

        if (g_InputDB.menu_up && ring->mode != INV_TITLE_MODE
            && ring->mode != INV_KEYS_MODE) {
            if (ring->type == RT_MAIN) {
                if (g_InvRing_Source[RT_KEYS].count != 0) {
                    InvRing_MotionSetup(
                        ring, RNG_CLOSING, RNG_MAIN2KEYS,
                        RINGSWITCH_FRAMES / 2);
                    InvRing_MotionRadius(ring, 0);
                    InvRing_MotionRotation(
                        ring, INV_RING_CLOSE_ROTATION,
                        ring->ring_pos.rot.y - INV_RING_CLOSE_ROTATION);
                    InvRing_MotionCameraPitch(ring, 0x2000);
                    ring->motion.misc = 0x2000;
                }
                g_Input = (INPUT_STATE) {};
                g_InputDB = (INPUT_STATE) {};
            } else if (ring->type == RT_OPTION) {
                if (g_InvRing_Source[RT_MAIN].count) {
                    InvRing_MotionSetup(
                        ring, RNG_CLOSING, RNG_OPTION2MAIN,
                        RINGSWITCH_FRAMES / 2);
                    InvRing_MotionRadius(ring, 0);
                    InvRing_MotionRotation(
                        ring, INV_RING_CLOSE_ROTATION,
                        ring->ring_pos.rot.y - INV_RING_CLOSE_ROTATION);
                    InvRing_MotionCameraPitch(ring, 0x2000);
                    ring->motion.misc = 0x2000;
                }
                g_InputDB = (INPUT_STATE) {};
            }
        } else if (
            g_InputDB.menu_down && ring->mode != INV_TITLE_MODE
            && ring->mode != INV_KEYS_MODE) {
            if (ring->type == RT_KEYS) {
                if (g_InvRing_Source[RT_MAIN].count) {
                    InvRing_MotionSetup(
                        ring, RNG_CLOSING, RNG_KEYS2MAIN,
                        RINGSWITCH_FRAMES / 2);
                    InvRing_MotionRadius(ring, 0);
                    InvRing_MotionRotation(
                        ring, INV_RING_CLOSE_ROTATION,
                        ring->ring_pos.rot.y - INV_RING_CLOSE_ROTATION);
                    InvRing_MotionCameraPitch(ring, -0x2000);
                    ring->motion.misc = -0x2000;
                }
                g_Input = (INPUT_STATE) {};
                g_InputDB = (INPUT_STATE) {};
            } else if (ring->type == RT_MAIN) {
                if (g_InvRing_Source[RT_OPTION].count != 0) {
                    InvRing_MotionSetup(
                        ring, RNG_CLOSING, RNG_MAIN2OPTION,
                        RINGSWITCH_FRAMES / 2);
                    InvRing_MotionRadius(ring, 0);
                    InvRing_MotionRotation(
                        ring, INV_RING_CLOSE_ROTATION,
                        ring->ring_pos.rot.y - INV_RING_CLOSE_ROTATION);
                    InvRing_MotionCameraPitch(ring, -0x2000);
                    ring->motion.misc = -0x2000;
                }
                g_InputDB = (INPUT_STATE) {};
            }
        }
        break;

    case RNG_MAIN2OPTION:
        InvRing_MotionSetup(ring, RNG_OPENING, RNG_OPEN, RINGSWITCH_FRAMES / 2);
        InvRing_MotionRadius(ring, INV_RING_RADIUS);
        ring->camera_pitch = -ring->motion.misc;
        ring->motion.camera_pitch_rate =
            ring->motion.misc / (RINGSWITCH_FRAMES / 2);
        ring->motion.camera_pitch_target = 0;
        g_InvRing_Source[RT_MAIN].current = ring->current_object;
        ring->type = RT_OPTION;
        ring->list = g_InvRing_Source[RT_OPTION].items;
        ring->number_of_objects = g_InvRing_Source[RT_OPTION].count;
        ring->current_object = g_InvRing_Source[RT_OPTION].current;
        InvRing_CalcAdders(ring, INV_RING_ROTATE_DURATION);
        InvRing_MotionRotation(
            ring, INV_RING_OPEN_ROTATION,
            -DEG_90 - ring->angle_adder * ring->current_object);
        ring->ring_pos.rot.y =
            ring->motion.rotate_target + INV_RING_OPEN_ROTATION;
        break;

    case RNG_MAIN2KEYS:
        InvRing_MotionSetup(ring, RNG_OPENING, RNG_OPEN, RINGSWITCH_FRAMES / 2);
        InvRing_MotionRadius(ring, INV_RING_RADIUS);
        ring->camera_pitch = -ring->motion.misc;
        ring->motion.camera_pitch_rate =
            ring->motion.misc / (RINGSWITCH_FRAMES / 2);
        ring->motion.camera_pitch_target = 0;
        g_InvRing_Source[RT_MAIN].current = ring->current_object;
        g_InvRing_Source[RT_MAIN].count = ring->number_of_objects;
        ring->type = RT_KEYS;
        ring->list = g_InvRing_Source[RT_KEYS].items;
        ring->number_of_objects = g_InvRing_Source[RT_KEYS].count;
        ring->current_object = g_InvRing_Source[RT_KEYS].current;
        InvRing_CalcAdders(ring, INV_RING_ROTATE_DURATION);
        InvRing_MotionRotation(
            ring, INV_RING_OPEN_ROTATION,
            -DEG_90 - ring->angle_adder * ring->current_object);
        ring->ring_pos.rot.y =
            ring->motion.rotate_target + INV_RING_OPEN_ROTATION;
        break;

    case RNG_KEYS2MAIN:
        InvRing_MotionSetup(ring, RNG_OPENING, RNG_OPEN, RINGSWITCH_FRAMES / 2);
        InvRing_MotionRadius(ring, INV_RING_RADIUS);
        ring->camera_pitch = -ring->motion.misc;
        ring->motion.camera_pitch_rate =
            ring->motion.misc / (RINGSWITCH_FRAMES / 2);
        ring->motion.camera_pitch_target = 0;
        g_InvRing_Source[RT_KEYS].current = ring->current_object;
        ring->type = RT_MAIN;
        ring->list = g_InvRing_Source[RT_MAIN].items;
        ring->number_of_objects = g_InvRing_Source[RT_MAIN].count;
        ring->current_object = g_InvRing_Source[RT_MAIN].current;
        InvRing_CalcAdders(ring, INV_RING_ROTATE_DURATION);
        InvRing_MotionRotation(
            ring, INV_RING_OPEN_ROTATION,
            -DEG_90 - ring->angle_adder * ring->current_object);
        ring->ring_pos.rot.y =
            ring->motion.rotate_target + INV_RING_OPEN_ROTATION;
        break;

    case RNG_OPTION2MAIN:
        InvRing_MotionSetup(ring, RNG_OPENING, RNG_OPEN, RINGSWITCH_FRAMES / 2);
        InvRing_MotionRadius(ring, INV_RING_RADIUS);
        ring->camera_pitch = -ring->motion.misc;
        ring->motion.camera_pitch_rate =
            ring->motion.misc / (RINGSWITCH_FRAMES / 2);
        ring->motion.camera_pitch_target = 0;
        g_InvRing_Source[RT_OPTION].count = ring->number_of_objects;
        g_InvRing_Source[RT_OPTION].current = ring->current_object;
        ring->type = RT_MAIN;
        ring->list = g_InvRing_Source[RT_MAIN].items;
        ring->number_of_objects = g_InvRing_Source[RT_MAIN].count;
        ring->current_object = g_InvRing_Source[RT_MAIN].current;
        InvRing_CalcAdders(ring, INV_RING_ROTATE_DURATION);
        InvRing_MotionRotation(
            ring, INV_RING_OPEN_ROTATION,
            -DEG_90 - ring->angle_adder * ring->current_object);
        ring->ring_pos.rot.y =
            ring->motion.rotate_target + INV_RING_OPEN_ROTATION;
        break;

    case RNG_SELECTED: {
        INVENTORY_ITEM *inv_item = ring->list[ring->current_object];
        if (inv_item->object_id == O_PASSPORT_CLOSED) {
            inv_item->object_id = O_PASSPORT_OPTION;
        }

        bool busy = false;
        for (int32_t frame = 0; frame < INV_RING_FRAMES; frame++) {
            busy = false;
            if (inv_item->y_rot == inv_item->y_rot_sel) {
                busy = M_AnimateInventoryItem(inv_item);
            }
        }

        Option_Control(inv_item, busy);

        if (!busy) {
            if (g_InputDB.menu_back) {
                InvRing_MotionSetup(ring, RNG_CLOSING_ITEM, RNG_DESELECT, 0);
                g_Input = (INPUT_STATE) {};
                g_InputDB = (INPUT_STATE) {};

                if (ring->mode == INV_LOAD_MODE || ring->mode == INV_SAVE_MODE
                    || ring->mode == INV_SAVE_CRYSTAL_MODE) {
                    InvRing_MotionSetup(
                        ring, RNG_CLOSING_ITEM, RNG_EXITING_INVENTORY, 0);
                    g_Input = (INPUT_STATE) {};
                    g_InputDB = (INPUT_STATE) {};
                }
            }

            if (g_InputDB.menu_confirm) {
                m_InvChosen = inv_item->object_id;
                if (ring->type == RT_MAIN) {
                    g_InvRing_Source[RT_MAIN].current = ring->current_object;
                } else {
                    g_InvRing_Source[RT_OPTION].current = ring->current_object;
                }

                if (ring->mode == INV_TITLE_MODE
                    && ((inv_item->object_id == O_DETAIL_OPTION)
                        || inv_item->object_id == O_SOUND_OPTION
                        || inv_item->object_id == O_CONTROL_OPTION
                        || inv_item->object_id == O_GAMMA_OPTION)) {
                    InvRing_MotionSetup(
                        ring, RNG_CLOSING_ITEM, RNG_DESELECT, 0);
                } else {
                    InvRing_MotionSetup(
                        ring, RNG_CLOSING_ITEM, RNG_EXITING_INVENTORY, 0);
                }
                g_Input = (INPUT_STATE) {};
                g_InputDB = (INPUT_STATE) {};
            }
        }
        break;
    }

    case RNG_DESELECT: {
        INVENTORY_ITEM *const inv_item = ring->list[ring->current_object];
        Option_Shutdown(inv_item);
        Sound_Effect(SFX_MENU_SPINOUT, nullptr, SPM_ALWAYS);
        InvRing_MotionSetup(ring, RNG_DESELECTING, RNG_OPEN, SELECTING_FRAMES);
        InvRing_MotionRotation(
            ring, 0, -DEG_90 - ring->angle_adder * ring->current_object);
        g_Input = (INPUT_STATE) {};
        g_InputDB = (INPUT_STATE) {};
        break;
    }

    case RNG_CLOSING_ITEM: {
        INVENTORY_ITEM *inv_item = ring->list[ring->current_object];
        for (int32_t frame = 0; frame < INV_RING_FRAMES; frame++) {
            if (!M_AnimateInventoryItem(inv_item)) {
                if (inv_item->object_id == O_PASSPORT_OPTION) {
                    inv_item->object_id = O_PASSPORT_CLOSED;
                    inv_item->current_frame = 0;
                }
                ring->motion.count = SELECTING_FRAMES;
                ring->motion.status = ring->motion.status_target;
                InvRing_MotionItemDeselect(ring, inv_item);
                break;
            }
        }
        break;
    }

    case RNG_EXITING_INVENTORY:
        if (!ring->motion.count) {
            if (M_Finish(ring, false).action != GF_NOOP) {
                // Fade to black. Do it later once reaching RNG_FADING_OUT.
                InvRing_MotionSetup(
                    ring, RNG_CLOSING, RNG_FADING_OUT, INV_RING_CLOSE_FRAMES);
            } else {
                // Fade to game. Do it as soon as the ring starts to close.
                InvRing_MotionSetup(
                    ring, RNG_CLOSING, RNG_DONE, INV_RING_CLOSE_FRAMES);
                Fader_Init(
                    &ring->back_fader, FADER_ANY, FADER_TRANSPARENT,
                    INV_RING_FADE_TIME_FAST);
            }
            InvRing_MotionRadius(ring, 0);
            InvRing_MotionCameraPos(ring, INV_RING_CAMERA_START_HEIGHT);
            InvRing_MotionRotation(
                ring, INV_RING_CLOSE_ROTATION,
                ring->ring_pos.rot.y - INV_RING_CLOSE_ROTATION);
        }
        break;

    default:
        break;
    }

    if (ring->motion.status == RNG_OPEN || ring->motion.status == RNG_SELECTING
        || ring->motion.status == RNG_SELECTED
        || ring->motion.status == RNG_DESELECTING
        || ring->motion.status == RNG_DESELECT
        || ring->motion.status == RNG_CLOSING_ITEM) {
        if (!ring->rotating && !g_Input.menu_left && !g_Input.menu_right) {
            INVENTORY_ITEM *const inv_item = ring->list[ring->current_object];
            M_RingNotActive(inv_item);
        }
        M_RingIsOpen(ring);
    } else {
        M_RingIsNotOpen(ring);
    }

    if (ring->motion.status == RNG_OPENING || ring->motion.status == RNG_CLOSING
        || ring->motion.status == RNG_MAIN2OPTION
        || ring->motion.status == RNG_OPTION2MAIN
        || ring->motion.status == RNG_EXITING_INVENTORY
        || ring->motion.status == RNG_FADING_OUT
        || ring->motion.status == RNG_DONE || ring->rotating) {
        M_RingActive(ring);
    }

    for (int32_t i = 0; i < ring->number_of_objects; i++) {
        InvRing_UpdateInventoryItem(ring, ring->list[i], INV_RING_FRAMES);
    }

    Interpolation_Remember();
    return (GF_COMMAND) { .action = GF_NOOP };
}

static bool M_CheckDemoTimer(const INV_RING *const ring)
{
    if (!g_Config.gameplay.enable_demo
        || GF_GetLevelTable(GFLT_DEMOS)->count == 0) {
        return false;
    }

    if (ring->mode != INV_TITLE_MODE || g_Input.any || g_InputDB.any
        || Console_IsOpened()) {
        ClockTimer_Sync(&m_DemoTimer);
        return false;
    }

    return ring->motion.status == RNG_OPEN
        && ClockTimer_CheckElapsed(&m_DemoTimer, g_GameFlow.demo_delay);
}

bool InvRing_CanExamine(void)
{
    return g_Config.gameplay.enable_item_examining
        && m_ExamineItemText != nullptr && !m_ExamineItemText->flags.hide;
}

void InvRing_RemoveAllText(void)
{
    InvRing_RemoveHeader();
    InvRing_RemoveItemTexts();
    M_RemoveExamineOverlay();
}

INV_RING *InvRing_Open(const INVENTORY_MODE mode)
{
    if (mode == INV_KEYS_MODE && g_InvRing_Source[RT_KEYS].count == 0) {
        m_InvChosen = NO_OBJECT;
        return nullptr;
    }

    g_PhdLeft = Viewport_GetMinX();
    g_PhdTop = Viewport_GetMinY();
    g_PhdBottom = Viewport_GetMaxY();
    g_PhdRight = Viewport_GetMaxX();
    m_InvChosen = NO_OBJECT;

    g_InvRing_OldCamera = g_Camera;
    m_StartLevel = -1;

    if (mode == INV_TITLE_MODE) {
        g_InvRing_Source[RT_OPTION].count = TITLE_RING_OBJECTS;
        InvRing_ShowVersionText();
    } else {
        g_InvRing_Source[RT_OPTION].count = OPTION_RING_OBJECTS;
        InvRing_RemoveVersionText();
    }

    g_InvRing_Source[RT_MAIN].current = 0;
    for (int32_t i = 0; i < g_InvRing_Source[RT_MAIN].count; i++) {
        InvRing_InitInvItem(g_InvRing_Source[RT_MAIN].items[i]);
    }
    g_InvRing_Source[RT_OPTION].current = 0;
    for (int32_t i = 0; i < g_InvRing_Source[RT_OPTION].count; i++) {
        g_InvRing_Source[RT_OPTION].qtys[i] = 1;
        InvRing_InitInvItem(g_InvRing_Source[RT_OPTION].items[i]);
    }
    if (GF_GetGymLevel() == nullptr) {
        Inv_RemoveItem(O_PHOTO_OPTION);
    }

    g_OptionSelected = 0;

    if (!g_Config.audio.enable_music_in_inventory && mode != INV_TITLE_MODE) {
        Music_Pause();
        Sound_PauseAll();
    } else {
        Sound_ResetAmbient();
        Sound_UpdateEffects();
    }

    INV_RING *const ring = Memory_Alloc(sizeof(INV_RING));
    ring->mode = mode;

    switch (mode) {
    case INV_DEATH_MODE:
    case INV_SAVE_MODE:
    case INV_SAVE_CRYSTAL_MODE:
    case INV_LOAD_MODE:
    case INV_TITLE_MODE:
        InvRing_InitRing(
            ring, RT_OPTION, g_InvRing_Source[RT_OPTION].items,
            g_InvRing_Source[RT_OPTION].count,
            g_InvRing_Source[RT_OPTION].current);
        break;

    case INV_KEYS_MODE:
        InvRing_InitRing(
            ring, RT_KEYS, g_InvRing_Source[RT_KEYS].items,
            g_InvRing_Source[RT_KEYS].count, g_InvRing_Source[RT_MAIN].current);
        break;

    default:
        if (g_InvRing_Source[RT_MAIN].count != 0) {
            InvRing_InitRing(
                ring, RT_MAIN, g_InvRing_Source[RT_MAIN].items,
                g_InvRing_Source[RT_MAIN].count,
                g_InvRing_Source[RT_MAIN].current);
        } else {
            InvRing_InitRing(
                ring, RT_OPTION, g_InvRing_Source[RT_OPTION].items,
                g_InvRing_Source[RT_OPTION].count,
                g_InvRing_Source[RT_OPTION].current);
        }
        break;
    }

    g_InvMode = mode;
    Interpolation_Remember();
    if (g_Config.gameplay.enable_timer_in_inventory) {
        Stats_StartTimer();
    }

    if (mode == INV_TITLE_MODE) {
        Output_LoadBackgroundFromFile(g_GameFlow.main_menu_background_path);
        Fader_Init(
            &ring->top_fader, FADER_BLACK, FADER_TRANSPARENT,
            INV_RING_FADE_TIME_FAST);
    } else {
        Output_UnloadBackground();
        Fader_Init(
            &ring->back_fader, FADER_TRANSPARENT, FADER_SEMI_BLACK,
            INV_RING_FADE_TIME_FAST);
    }

    g_GameInfo.inv_ring_shown = true;
    return ring;
}

void InvRing_Close(INV_RING *const ring)
{
    InvRing_RemoveAllText();
    InvRing_RemoveVersionText();

    if (ring->list != nullptr) {
        INVENTORY_ITEM *const inv_item = ring->list[ring->current_object];
        if (inv_item != nullptr) {
            Option_Shutdown(inv_item);
        }
    }
    if (ring->mode == INV_TITLE_MODE) {
        Music_Stop();
        Sound_StopAll();
    }
    Output_UnloadBackground();

    if (g_Config.gameplay.fix_item_duplication_glitch) {
        Inv_ClearSelection();
    }
    if (g_Config.input.enable_buffering) {
        g_OldInputDB = (INPUT_STATE) {};
    }

    g_GameInfo.inv_ring_shown = false;
    m_InvChosen = NO_OBJECT;
    Memory_Free(ring);
}

GF_COMMAND InvRing_Control(INV_RING *const ring, const int32_t num_frames)
{
    GF_COMMAND gf_cmd = { .action = GF_NOOP };
    for (int32_t i = 0; i < num_frames; i++) {
        gf_cmd = M_Control(ring);
        if (gf_cmd.action != GF_NOOP) {
            break;
        }
    }

    return gf_cmd;
}

bool InvRing_IsOptionLockedOut(void)
{
    return false;
}
