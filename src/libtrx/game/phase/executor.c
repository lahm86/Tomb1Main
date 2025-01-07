#include "game/phase/executor.h"

#include "game/clock.h"
#include "game/game.h"
#include "game/gameflow.h"
#include "game/interpolation.h"
#include "game/output.h"

#include <stdbool.h>
#include <stddef.h>

#define MAX_PHASES 10

static int32_t m_PhaseStackSize = 0;
static PHASE *m_PhaseStack[MAX_PHASES] = {};

static PHASE_CONTROL M_Control(PHASE *phase, int32_t nframes);
static void M_Draw(PHASE *phase);
static int32_t M_Wait(PHASE *phase);

static PHASE_CONTROL M_Control(PHASE *const phase, const int32_t nframes)
{
    const GAME_FLOW_COMMAND gf_override_cmd = GameFlow_GetOverrideCommand();
    if (gf_override_cmd.action != GF_NOOP) {
        const GAME_FLOW_COMMAND gf_cmd = gf_override_cmd;
        GameFlow_OverrideCommand((GAME_FLOW_COMMAND) { .action = GF_NOOP });
        return (PHASE_CONTROL) { .action = PHASE_ACTION_END, .gf_cmd = gf_cmd };
    }
    if (phase != NULL && phase->control != NULL) {
        return phase->control(phase, nframes);
    }
    return (PHASE_CONTROL) {
        .action = PHASE_ACTION_END,
        .gf_cmd = { .action = GF_NOOP },
    };
}

static void M_Draw(PHASE *const phase)
{
    Output_BeginScene();
    if (phase != NULL && phase->draw != NULL) {
        phase->draw(phase);
    }
    Output_EndScene();
}

static int32_t M_Wait(PHASE *const phase)
{
    if (phase != NULL && phase->wait != NULL) {
        return phase->wait(phase);
    } else {
        return Clock_WaitTick();
    }
}

GAME_FLOW_COMMAND PhaseExecutor_Run(PHASE *const phase)
{
    GAME_FLOW_COMMAND gf_cmd = { .action = GF_NOOP };

    PHASE *const prev_phase =
        m_PhaseStackSize > 0 ? m_PhaseStack[m_PhaseStackSize - 1] : NULL;
    if (prev_phase != NULL && prev_phase->suspend != NULL) {
        prev_phase->suspend(phase);
    }
    m_PhaseStack[m_PhaseStackSize++] = phase;

    if (phase->start != NULL) {
        Clock_SyncTick();
        const PHASE_CONTROL control = phase->start(phase);
        if (Game_IsExiting()) {
            gf_cmd = (GAME_FLOW_COMMAND) { .action = GF_EXIT_GAME };
            goto finish;
        } else if (control.action == PHASE_ACTION_END) {
            gf_cmd = control.gf_cmd;
            goto finish;
        }
    }

    int32_t nframes = Clock_WaitTick();
    while (true) {
        const PHASE_CONTROL control = M_Control(phase, nframes);

        if (control.action == PHASE_ACTION_END) {
            if (Game_IsExiting()) {
                gf_cmd = (GAME_FLOW_COMMAND) { .action = GF_EXIT_GAME };
            } else {
                gf_cmd = control.gf_cmd;
            }
            goto finish;
        } else if (control.action == PHASE_ACTION_NO_WAIT) {
            nframes = 0;
            continue;
        } else {
            nframes = 0;
            if (Interpolation_IsEnabled()) {
                Interpolation_SetRate(0.5);
                M_Draw(phase);
                M_Wait(phase);
            }

            Interpolation_SetRate(1.0);
            M_Draw(phase);
            nframes += M_Wait(phase);
        }
    }

finish:
    if (phase->end != NULL) {
        phase->end(phase);
    }
    if (prev_phase != NULL && prev_phase->resume != NULL) {
        Clock_SyncTick();
        prev_phase->resume(phase);
    }
    m_PhaseStackSize--;

    return gf_cmd;
}