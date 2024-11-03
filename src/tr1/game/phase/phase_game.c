#include "game/phase/phase_game.h"

#include "game/camera.h"
#include "game/clock.h"
#include "game/console/common.h"
#include "game/effects.h"
#include "game/game.h"
#include "game/input.h"
#include "game/interpolation.h"
#include "game/inventory.h"
#include "game/items.h"
#include "game/lara/cheat.h"
#include "game/lara/common.h"
#include "game/lara/hair.h"
#include "game/output.h"
#include "game/overlay.h"
#include "game/phase/phase_photo_mode.h"
#include "game/random.h"
#include "game/shell.h"
#include "game/sound.h"
#include "game/stats.h"
#include "game/text.h"
#include "global/const.h"
#include "global/types.h"
#include "global/vars.h"

#include <libtrx/filesystem.h>
#include <libtrx/memory.h>
#include <libtrx/utils.h>
#include <libtrx/vector.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define DEMO_DIR "demos"
#define DEMO_EXT "dmo"

static struct {
    bool enable_enhanced_look;
    bool enable_tr2_swimming;
    bool fix_bear_ai;
    TARGET_LOCK_MODE target_mode;
    int32_t fps;
} m_OldConfig;

static void M_Start(const void *args);
static void M_End(void);
static PHASE_CONTROL M_Control(int32_t nframes);
static void M_Draw(void);

static void M_Start(const void *const args)
{
    Interpolation_Remember();
    Stats_StartTimer();
    if (Phase_Get() != PHASE_PAUSE) {
        Output_FadeReset();
    }
}

static void M_End(void)
{
}

#define RECORD(val)                                                            \
    do {                                                                       \
        uint32_t data = (uint32_t)(val);                                       \
        Vector_Add(m_RecordData, (void *)&data);                               \
    } while (0)

static VECTOR *m_RecordData = NULL;

static bool M_IsRecording(void)
{
    return m_RecordData != NULL;
}

static void M_UpdateRecording(void)
{
    if (M_IsRecording()) {
        if (g_InputDB.toggle_recording || m_RecordData->count == DEMO_COUNT_MAX
            || g_LaraItem->hit_points <= 0) {
            RECORD(-1);

            File_CreateDirectory(DEMO_DIR);

            char date_time[20];
            Clock_GetDateTime(date_time);

            size_t out_size =
                snprintf(NULL, 0, "%s-Level-%d", date_time, g_CurrentLevel) + 1;
            char *filename = Memory_Alloc(out_size);
            snprintf(
                filename, out_size, "%s-Level-%d", date_time, g_CurrentLevel);

            char *full_path = Memory_Alloc(
                strlen(DEMO_DIR) + strlen(filename) + strlen(DEMO_EXT) + 6);
            sprintf(full_path, "%s/%s.%s", DEMO_DIR, filename, DEMO_EXT);

            MYFILE *const fp = File_Open(full_path, FILE_OPEN_WRITE);
            File_WriteU16(fp, (uint16_t)(m_RecordData->count + 1));
            for (int32_t i = 0; i < m_RecordData->count; i++) {
                File_WriteU32(
                    fp, *(const uint32_t *)Vector_Get(m_RecordData, i));
            }

            Console_Log("Demo saved to %s", filename);

            Memory_FreePointer(&filename);
            Memory_FreePointer(&full_path);
            File_Close(fp);

            Vector_Free(m_RecordData);
            m_RecordData = NULL;

            g_Config.target_mode = m_OldConfig.target_mode;
            g_Config.enable_enhanced_look = m_OldConfig.enable_enhanced_look;
            g_Config.enable_tr2_swimming = m_OldConfig.enable_tr2_swimming;
            g_Config.fix_bear_ai = m_OldConfig.fix_bear_ai;
            g_Config.rendering.fps = m_OldConfig.fps;

        } else {
            RECORD(g_Input.any & 0xF01FFF);
        }
    } else if (g_InputDB.toggle_recording) {
        if (g_Config.enable_tr2_jumping) {
            Console_Log(
                "TR2 jumping is enabled - please disable and restart the "
                "level.");
            return;
        }

        m_OldConfig.enable_enhanced_look = g_Config.enable_enhanced_look;
        m_OldConfig.enable_tr2_swimming = g_Config.enable_tr2_swimming;
        m_OldConfig.target_mode = g_Config.target_mode;
        m_OldConfig.fix_bear_ai = g_Config.fix_bear_ai;
        m_OldConfig.fps = g_Config.rendering.fps;
        g_Config.enable_enhanced_look = false;
        g_Config.enable_tr2_swimming = false;
        g_Config.target_mode = TLM_FULL;
        g_Config.fix_bear_ai = false;
        g_Config.rendering.fps = 30; // not sure if needed

        m_RecordData =
            Vector_CreateAtCapacity(sizeof(uint32_t), DEMO_COUNT_MAX / 2);
        RECORD(g_LaraItem->pos.x);
        RECORD(g_LaraItem->pos.y);
        RECORD(g_LaraItem->pos.z);
        RECORD(g_LaraItem->rot.x);
        RECORD(g_LaraItem->rot.y);
        RECORD(g_LaraItem->rot.z);
        RECORD(g_LaraItem->room_num);

        Random_SeedDraw(0xD371F947);
        Random_SeedControl(0xD371F947);

        Console_Log("Recording!");
    }
}

static PHASE_CONTROL M_Control(int32_t nframes)
{
    Interpolation_Remember();
    Stats_UpdateTimer();
    CLAMPG(nframes, MAX_FRAMES);

    for (int32_t i = 0; i < nframes; i++) {
        Lara_Cheat_Control();
        if (g_LevelComplete) {
            return (PHASE_CONTROL) {
                .end = true,
                .command = { .action = GF_CONTINUE_SEQUENCE },
            };
        }

        Input_Update();
        Shell_ProcessInput();
        Game_ProcessInput();
        M_UpdateRecording();

        if (g_Lara.death_timer > DEATH_WAIT
            || (g_Lara.death_timer > DEATH_WAIT_MIN && g_Input.any
                && !g_Input.fly_cheat)
            || g_OverlayFlag == 2) {
            if (g_OverlayFlag == 2) {
                g_OverlayFlag = 1;
                Inv_Display(INV_DEATH_MODE);
                return (PHASE_CONTROL) { .end = false };
            } else {
                g_OverlayFlag = 2;
            }
        }

        if ((g_InputDB.option || g_Input.save || g_Input.load
             || g_OverlayFlag <= 0)
            && !g_Lara.death_timer) {
            if (g_Camera.type == CAM_CINEMATIC) {
                g_OverlayFlag = 0;
            } else if (g_OverlayFlag > 0) {
                if (g_Input.load) {
                    g_OverlayFlag = -1;
                } else if (g_Input.save) {
                    g_OverlayFlag = -2;
                } else {
                    g_OverlayFlag = 0;
                }
            } else {
                if (g_OverlayFlag == -1) {
                    Inv_Display(INV_LOAD_MODE);
                } else if (g_OverlayFlag == -2) {
                    Inv_Display(INV_SAVE_MODE);
                } else {
                    Inv_Display(INV_GAME_MODE);
                }

                g_OverlayFlag = 1;
                return (PHASE_CONTROL) { .end = false };
            }
        }

        if (!g_Lara.death_timer && g_InputDB.pause) {
            Phase_Set(PHASE_PAUSE, NULL);
            return (PHASE_CONTROL) { .end = false };
        } else if (g_InputDB.toggle_photo_mode) {
            PHASE_PHOTO_MODE_ARGS *const args =
                Memory_Alloc(sizeof(PHASE_PHOTO_MODE_ARGS));
            args->phase_to_return_to = PHASE_GAME;
            args->phase_arg = NULL;
            Phase_Set(PHASE_PHOTO_MODE, args);
            return (PHASE_CONTROL) { .end = false };
        } else {
            Item_Control();
            Effect_Control();

            Lara_Control();
            Lara_Hair_Control();

            Camera_Update();
            Sound_ResetAmbient();
            Effect_RunActiveFlipEffect();
            Sound_UpdateEffects();
            Overlay_BarHealthTimerTick();
        }
    }

    if (g_GameInfo.ask_for_save) {
        Inv_Display(INV_SAVE_CRYSTAL_MODE);
        g_GameInfo.ask_for_save = false;
    }

    return (PHASE_CONTROL) { .end = false };
}

static void M_Draw(void)
{
    Game_DrawScene(true);
    Output_AnimateTextures();
    Output_AnimateFades();
    Text_Draw();
}

PHASER g_GamePhaser = {
    .start = M_Start,
    .end = M_End,
    .control = M_Control,
    .draw = M_Draw,
    .wait = NULL,
};
