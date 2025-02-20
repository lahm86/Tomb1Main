#include "decomp/decomp.h"
#include "game/camera.h"
#include "game/collide.h"
#include "game/effects.h"
#include "game/input.h"
#include "game/items.h"
#include "game/lara/look.h"
#include "game/objects/common.h"
#include "game/objects/traps/gondola.h"
#include "game/output.h"
#include "game/random.h"
#include "game/room.h"
#include "game/sound.h"
#include "game/spawn.h"
#include "global/vars.h"

#include <libtrx/game/game_buf.h>
#include <libtrx/game/math.h>
#include <libtrx/game/matrix.h>
#include <libtrx/utils.h>

#define BOAT_FALL_ANIM 15
#define BOAT_DEATH_ANIM 18
#define BOAT_GET_ON_LW_ANIM 0
#define BOAT_GET_ON_RW_ANIM 8
#define BOAT_GET_ON_J_ANIM 6
#define BOAT_GET_ON_START 1

#define LF_BOAT_EXIT_END 24

#define BOAT_RADIUS 500
#define BOAT_SIDE 300
#define BOAT_FRONT 750
#define BOAT_TIP (BOAT_FRONT + 250)
#define BOAT_MIN_SPEED 20
#define BOAT_MAX_SPEED 90
#define BOAT_SLOW_SPEED (BOAT_MAX_SPEED / 3) // = 30
#define BOAT_FAST_SPEED (BOAT_MAX_SPEED + 50) // = 140
#define BOAT_MAX_BACK (-20)
#define BOAT_ACCELERATION 5
#define BOAT_BRAKE 5
#define BOAT_REVERSE (-5)
#define BOAT_SLOWDOWN 1
#define BOAT_WAKE 700
#define BOAT_UNDO_TURN (DEG_1 / 4) // = 45
#define BOAT_TURN (DEG_1 / 8) // = 22
#define BOAT_MAX_TURN (DEG_1 * 4) // = 728
#define BOAT_SOUND_CEILING (WALL_L * 5) // = 5120

typedef enum {
    BOAT_STATE_GET_ON = 0,
    BOAT_STATE_STILL = 1,
    BOAT_STATE_MOVING = 2,
    BOAT_STATE_JUMP_R = 3,
    BOAT_STATE_JUMP_L = 4,
    BOAT_STATE_HIT = 5,
    BOAT_STATE_FALL = 6,
    BOAT_STATE_DEATH = 8,
} BOAT_STATE;

static int32_t M_CheckGetOn(
    const int16_t item_num, const COLL_INFO *const coll);
static int32_t M_TestWaterHeight(
    const ITEM *const item, const int32_t z_off, const int32_t x_off,
    XYZ_32 *const pos);
static void M_DoWakeEffect(const ITEM *const boat);
static void M_DoShift(const int32_t boat_num);
static int32_t M_DoDynamics(
    const int32_t height, int32_t fall_speed, int32_t *const y);
static int32_t M_Dynamics(const int16_t boat_num);
static int32_t M_UserControl(ITEM *const boat);
static void M_Animation(const ITEM *const boat, const int32_t collide);
static void M_Setup(OBJECT *const obj);
static void M_Initialise(const int16_t item_num);
static void M_Collision(
    const int16_t item_num, ITEM *const lara, COLL_INFO *const coll);
static void M_Control(const int16_t item_num);

static int32_t M_CheckGetOn(const int16_t item_num, const COLL_INFO *const coll)
{
    if (g_Lara.gun_status != LGS_ARMLESS) {
        return 0;
    }

    ITEM *const boat = Item_Get(item_num);
    const ITEM *const lara = g_LaraItem;
    const int32_t dist =
        ((lara->pos.z - boat->pos.z) * Math_Cos(-boat->rot.y)
         - (lara->pos.x - boat->pos.x) * Math_Sin(-boat->rot.y))
        >> W2V_SHIFT;

    if (dist > 200) {
        return 0;
    }

    int32_t get_on = 0;
    const int16_t rot = boat->rot.y - lara->rot.y;

    if (g_Lara.water_status == LWS_SURFACE || g_Lara.water_status == LWS_WADE) {
        if (!g_Input.action || lara->gravity || boat->speed) {
            return 0;
        }

        if (rot > DEG_45 && rot < DEG_135) {
            get_on = 1;
        } else if (rot > -DEG_135 && rot < -DEG_45) {
            get_on = 2;
        }
    } else if (g_Lara.water_status == LWS_ABOVE_WATER) {
        int16_t fall_speed = lara->fall_speed;
        if (fall_speed > 0) {
            if (rot > -DEG_135 && rot < DEG_135 && lara->pos.y > boat->pos.y) {
                get_on = 3;
            }
        } else if (!fall_speed && rot > -DEG_135 && rot < DEG_135) {
            if (lara->pos.x == boat->pos.x && lara->pos.y == boat->pos.y
                && lara->pos.z == boat->pos.z) {
                get_on = 4;
            } else {
                get_on = 3;
            }
        }
    }

    if (!get_on) {
        return 0;
    }

    if (!Item_TestBoundsCollide(boat, lara, coll->radius)) {
        return 0;
    }

    if (!Collide_TestCollision(boat, lara)) {
        return 0;
    }

    return get_on;
}

static int32_t M_TestWaterHeight(
    const ITEM *const item, const int32_t z_off, const int32_t x_off,
    XYZ_32 *const pos)
{
    // clang-format off
    pos->y = item->pos.y
        + ((x_off * Math_Sin(item->rot.z)) >> W2V_SHIFT)
        - ((z_off * Math_Sin(item->rot.x)) >> W2V_SHIFT);
    // clang-format on

    const int32_t c = Math_Cos(item->rot.y);
    const int32_t s = Math_Sin(item->rot.y);
    pos->x = item->pos.x + ((x_off * c + z_off * s) >> W2V_SHIFT);
    pos->z = item->pos.z + ((z_off * c - x_off * s) >> W2V_SHIFT);

    int16_t room_num = item->room_num;
    Room_GetSector(pos->x, pos->y, pos->z, &room_num);
    int32_t height = Room_GetWaterHeight(pos->x, pos->y, pos->z, room_num);
    if (height == NO_HEIGHT) {
        const SECTOR *const sector =
            Room_GetSector(pos->x, pos->y, pos->z, &room_num);
        height = Room_GetHeight(sector, pos->x, pos->y, pos->z);
        if (height != NO_HEIGHT) {
            return height;
        }
    }

    return height - 5;
}

static void M_DoWakeEffect(const ITEM *const boat)
{
    g_MatrixPtr->_23 = 0;
    Output_CalculateLight(boat->pos, boat->room_num);

    const int16_t frame =
        (Random_GetDraw() * Object_Get(O_WATER_SPRITE)->mesh_count) >> 15;

    for (int32_t i = 0; i < 3; i++) {
        const int16_t effect_num = Effect_Create(boat->room_num);
        if (effect_num == NO_EFFECT) {
            continue;
        }

        EFFECT *const effect = Effect_Get(effect_num);
        effect->object_id = O_WATER_SPRITE;
        effect->room_num = boat->room_num;
        effect->frame_num = frame;

        const int32_t c = Math_Cos(boat->rot.y);
        const int32_t s = Math_Sin(boat->rot.y);
        const int32_t w = (1 - i) * BOAT_SIDE;
        const int32_t h = BOAT_WAKE;
        effect->pos.x = boat->pos.x + ((-c * w - s * h) >> W2V_SHIFT);
        effect->pos.y = boat->pos.y;
        effect->pos.z = boat->pos.z + ((-c * h + s * w) >> W2V_SHIFT);
        effect->rot.y = boat->rot.y + (i << W2V_SHIFT) - DEG_90;

        effect->counter = 20;
        effect->speed = boat->speed >> 2;
        if (boat->speed < 64) {
            effect->fall_speed =
                (Random_GetDraw() * (ABS(boat->speed) - 64)) >> 15;
        } else {
            effect->fall_speed = 0;
        }

        effect->shade = Output_GetLightAdder() - 768;
        CLAMPL(effect->shade, 0);
    }
}

static void M_DoShift(const int32_t boat_num)
{
    ITEM *const boat = Item_Get(boat_num);
    int16_t item_num = Room_Get(boat->room_num)->item_num;

    while (item_num != NO_ITEM) {
        ITEM *const item = Item_Get(item_num);

        if (item->object_id == O_BOAT && item_num != boat_num
            && g_Lara.skidoo != item_num) {
            const int32_t dx = item->pos.x - boat->pos.x;
            const int32_t dz = item->pos.z - boat->pos.z;
            const int32_t dist = SQUARE(dx) + SQUARE(dz);

            if (dist < SQUARE(BOAT_RADIUS * 2)) {
                boat->pos.x = item->pos.x - SQUARE(BOAT_RADIUS * 2) * dx / dist;
                boat->pos.z = item->pos.z - SQUARE(BOAT_RADIUS * 2) * dz / dist;
            }
            break;
        }

        if (item->object_id == O_GONDOLA
            && item->current_anim_state == GONDOLA_STATE_FLOATING) {
            const int32_t c = Math_Cos(item->rot.y);
            const int32_t s = Math_Sin(item->rot.y);
            const int32_t ix = item->pos.x - ((s * STEP_L * 2) >> W2V_SHIFT);
            const int32_t iz = item->pos.z - ((c * STEP_L * 2) >> W2V_SHIFT);
            const int32_t dx = ix - boat->pos.x;
            const int32_t dz = iz - boat->pos.z;
            const int32_t dist = SQUARE(dx) + SQUARE(dz);

            if (dist < SQUARE(BOAT_RADIUS * 2)) {
                if (boat->speed < BOAT_MAX_SPEED - 10) {
                    boat->pos.x = ix - SQUARE(BOAT_RADIUS * 2) * dx / dist;
                    boat->pos.z = iz - SQUARE(BOAT_RADIUS * 2) * dz / dist;
                } else if (item->pos.y - boat->pos.y < WALL_L * 2) {
                    Sound_Effect(SFX_BOAT_INTO_WATER, &item->pos, SPM_NORMAL);
                    item->goal_anim_state = GONDOLA_STATE_CRASH;
                }
            }
        }

        item_num = item->next_item;
    }
}

static int32_t M_DoDynamics(
    const int32_t height, int32_t fall_speed, int32_t *const y)
{
    if (height > *y) {
        *y = fall_speed + *y;
        if (*y > height) {
            *y = height;
            fall_speed = 0;
        } else {
            fall_speed += GRAVITY;
        }
    } else {
        fall_speed += ((height - fall_speed - *y) >> 3);
        CLAMPL(fall_speed, -20);
        CLAMPG(*y, height);
    }

    return fall_speed;
}

static int32_t M_Dynamics(const int16_t boat_num)
{
    ITEM *const boat = Item_Get(boat_num);
    BOAT_INFO *const boat_data = (BOAT_INFO *)boat->data;
    boat->rot.z -= boat_data->tilt_angle;

    XYZ_32 fl_old;
    XYZ_32 bl_old;
    XYZ_32 fr_old;
    XYZ_32 br_old;
    XYZ_32 f_old;
    const int32_t hfl_old =
        M_TestWaterHeight(boat, BOAT_FRONT, -BOAT_SIDE, &fl_old);
    const int32_t hfr_old =
        M_TestWaterHeight(boat, BOAT_FRONT, BOAT_SIDE, &fr_old);
    const int32_t hbl_old =
        M_TestWaterHeight(boat, -BOAT_FRONT, -BOAT_SIDE, &bl_old);
    const int32_t hbr_old =
        M_TestWaterHeight(boat, -BOAT_FRONT, BOAT_SIDE, &br_old);
    const int32_t hf_old = M_TestWaterHeight(boat, BOAT_TIP, 0, &f_old);
    XYZ_32 old = boat->pos;
    CLAMPG(bl_old.y, hbl_old);
    CLAMPG(br_old.y, hbr_old);
    CLAMPG(fl_old.y, hfl_old);
    CLAMPG(fr_old.y, hfr_old);
    CLAMPG(f_old.y, hf_old);

    boat->rot.y += boat_data->extra_rotation + boat_data->boat_turn;
    boat_data->tilt_angle = boat_data->boat_turn * 6;

    boat->pos.z += (boat->speed * Math_Cos(boat->rot.y)) >> W2V_SHIFT;
    boat->pos.x += (boat->speed * Math_Sin(boat->rot.y)) >> W2V_SHIFT;

    int32_t slip = (Math_Sin(boat->rot.z) * 30) >> W2V_SHIFT;
    if (!slip && boat->rot.z) {
        slip = boat->rot.z > 0 ? 1 : -1;
    }
    boat->pos.z -= (slip * Math_Sin(boat->rot.y)) >> W2V_SHIFT;
    boat->pos.x += (slip * Math_Cos(boat->rot.y)) >> W2V_SHIFT;

    slip = (Math_Sin(boat->rot.x) * 10) >> W2V_SHIFT;
    if (!slip && boat->rot.x) {
        slip = boat->rot.x > 0 ? 1 : -1;
    }

    boat->pos.z -= (slip * Math_Cos(boat->rot.y)) >> W2V_SHIFT;
    boat->pos.x = boat->pos.x - ((slip * Math_Sin(boat->rot.y)) >> W2V_SHIFT);

    XYZ_32 moved = {
        .x = boat->pos.x,
        .y = 0,
        .z = boat->pos.z,
    };
    M_DoShift(boat_num);

    int32_t rot = 0;

    XYZ_32 bl;
    const int32_t hbl = M_TestWaterHeight(boat, -BOAT_FRONT, -BOAT_SIDE, &bl);
    if (hbl < bl_old.y - STEP_L / 2) {
        rot = DoShift(boat, &bl, &bl_old);
    }

    XYZ_32 br;
    const int32_t hbr = M_TestWaterHeight(boat, -BOAT_FRONT, BOAT_SIDE, &br);
    if (hbr < br_old.y - STEP_L / 2) {
        rot += DoShift(boat, &br, &br_old);
    }

    XYZ_32 fl;
    const int32_t hfl = M_TestWaterHeight(boat, BOAT_FRONT, -BOAT_SIDE, &fl);
    if (hfl < fl_old.y - STEP_L / 2) {
        rot += DoShift(boat, &fl, &fl_old);
    }

    XYZ_32 fr;
    const int32_t hfr = M_TestWaterHeight(boat, BOAT_FRONT, BOAT_SIDE, &fr);
    if (hfr < fr_old.y - STEP_L / 2) {
        rot += DoShift(boat, &fr, &fr_old);
    }

    if (!slip) {
        XYZ_32 f;
        const int32_t hf = M_TestWaterHeight(boat, BOAT_TIP, 0, &f);
        if (hf < f_old.y - STEP_L / 2) {
            DoShift(boat, &f, &f_old);
        }
    }

    int16_t room_num = boat->room_num;
    const SECTOR *const sector =
        Room_GetSector(boat->pos.x, boat->pos.y, boat->pos.z, &room_num);
    int32_t height =
        Room_GetWaterHeight(boat->pos.x, boat->pos.y, boat->pos.z, room_num);
    if (height == NO_HEIGHT) {
        height = Room_GetHeight(sector, boat->pos.x, boat->pos.y, boat->pos.z);
    }
    if (height < boat->pos.y - STEP_L / 2) {
        DoShift(boat, &boat->pos, &old);
    }

    boat_data->extra_rotation = rot;

    const int32_t collide = GetCollisionAnim(boat, &moved);
    if (slip || collide) {
        // clang-format off
        const int32_t new_speed = (
            (boat->pos.z - old.z) * Math_Cos(boat->rot.y) +
            (boat->pos.x - old.x) * Math_Sin(boat->rot.y)
        ) >> W2V_SHIFT;
        // clang-format on

        if (g_Lara.skidoo == boat_num) {
            if (boat->speed > BOAT_MAX_SPEED + BOAT_ACCELERATION
                && new_speed < boat->speed - 10) {
                g_LaraItem->hit_points -= (boat->speed - new_speed) / 2;
                g_LaraItem->hit_status = 1;
                Sound_Effect(SFX_LARA_INJURY, &g_LaraItem->pos, SPM_NORMAL);
            }
        }

        if (slip) {
            if (boat->speed <= BOAT_MAX_SPEED + 10) {
                boat->speed = new_speed;
            }
        } else {
            if (boat->speed > 0 && new_speed < boat->speed) {
                boat->speed = new_speed;
            } else if (boat->speed < 0 && new_speed > boat->speed) {
                boat->speed = new_speed;
            }
        }

        CLAMPL(boat->speed, BOAT_MAX_BACK);
    }

    return collide;
}

static int32_t M_UserControl(ITEM *const boat)
{
    int32_t no_turn = 1;

    BOAT_INFO *const boat_data = (BOAT_INFO *)boat->data;
    if (boat->pos.y < boat_data->water - STEP_L / 2
        || boat_data->water == NO_HEIGHT) {
        return no_turn;
    }

    if (g_Input.look && !boat->speed) {
        Lara_LookUpDown();
        return no_turn;
    }

    if (g_Input.jump) {
        return no_turn;
    }

    if ((g_Input.left && !g_Input.back) || (g_Input.right && g_Input.back)) {
        if (boat_data->boat_turn > 0) {
            boat_data->boat_turn -= BOAT_UNDO_TURN;
        } else {
            boat_data->boat_turn -= BOAT_TURN;
            CLAMPL(boat_data->boat_turn, -BOAT_MAX_TURN);
        }
        no_turn = 0;
    } else if (
        (g_Input.right && !g_Input.back) || (g_Input.left && g_Input.back)) {
        if (boat_data->boat_turn < 0) {
            boat_data->boat_turn += BOAT_UNDO_TURN;
        } else {
            boat_data->boat_turn += BOAT_TURN;
            CLAMPG(boat_data->boat_turn, BOAT_MAX_TURN);
        }
        no_turn = 0;
    }

    if (g_Input.back) {
        if (boat->speed > 0) {
            boat->speed -= BOAT_BRAKE;
        } else if (boat->speed > BOAT_MAX_BACK) {
            boat->speed += BOAT_REVERSE;
        }
    } else if (g_Input.forward) {
        int32_t max_speed;
        if (g_Input.action) {
            max_speed = BOAT_FAST_SPEED;
        } else {
            max_speed = g_Input.slow ? BOAT_SLOW_SPEED : BOAT_MAX_SPEED;
        }

        if (boat->speed < max_speed) {
            boat->speed += BOAT_ACCELERATION / 2
                + BOAT_ACCELERATION * boat->speed / (2 * max_speed);
        } else if (boat->speed > max_speed + BOAT_SLOWDOWN) {
            boat->speed -= BOAT_SLOWDOWN;
        }
    } else if (
        boat->speed >= 0 && boat->speed < BOAT_MIN_SPEED
        && (g_Input.left || g_Input.right)) {
        boat->speed = BOAT_MIN_SPEED;
    } else if (boat->speed > BOAT_SLOWDOWN) {
        boat->speed -= BOAT_SLOWDOWN;
    } else {
        boat->speed = 0;
    }

    return no_turn;
}

static void M_Animation(const ITEM *const boat, const int32_t collide)
{
    ITEM *const lara = g_LaraItem;
    const BOAT_INFO *const boat_data = (const BOAT_INFO *)boat->data;

    if (lara->hit_points <= 0) {
        if (lara->current_anim_state == BOAT_STATE_DEATH) {
            return;
        }
        Item_SwitchToObjAnim(lara, BOAT_DEATH_ANIM, 0, O_LARA_BOAT);
        lara->goal_anim_state = BOAT_STATE_DEATH;
        lara->current_anim_state = BOAT_STATE_DEATH;
        return;
    }

    if (boat->pos.y < boat_data->water - STEP_L / 2 && boat->fall_speed > 0) {
        if (lara->current_anim_state == BOAT_STATE_FALL) {
            return;
        }
        Item_SwitchToObjAnim(lara, BOAT_FALL_ANIM, 0, O_LARA_BOAT);
        lara->goal_anim_state = BOAT_STATE_FALL;
        lara->current_anim_state = BOAT_STATE_FALL;
        return;
    }

    if (collide) {
        if (lara->current_anim_state == BOAT_STATE_HIT) {
            return;
        }
        Item_SwitchToObjAnim(lara, collide, 0, O_LARA_BOAT);
        lara->goal_anim_state = BOAT_STATE_HIT;
        lara->current_anim_state = BOAT_STATE_HIT;
        return;
    }

    switch (lara->current_anim_state) {
    case BOAT_STATE_STILL:
        if (g_Input.jump) {
            if (g_Input.right) {
                lara->goal_anim_state = BOAT_STATE_JUMP_R;
            } else if (g_Input.left) {
                lara->goal_anim_state = BOAT_STATE_JUMP_L;
            }
        }

        if (boat->speed > 0) {
            lara->goal_anim_state = BOAT_STATE_MOVING;
        }
        break;

    case BOAT_STATE_MOVING:
        if (g_Input.jump) {
            if (g_Input.right) {
                lara->goal_anim_state = BOAT_STATE_JUMP_R;
            } else if (g_Input.left) {
                lara->goal_anim_state = BOAT_STATE_JUMP_L;
            }
        } else if (boat->speed <= 0) {
            lara->goal_anim_state = BOAT_STATE_STILL;
        }
        break;

    case BOAT_STATE_FALL:
        lara->goal_anim_state = BOAT_STATE_MOVING;
        break;
    }
}

static void M_Setup(OBJECT *const obj)
{
    obj->initialise_func = M_Initialise;
    obj->control_func = M_Control;
    obj->collision_func = M_Collision;
    obj->save_position = 1;
    obj->save_flags = 1;
    obj->save_anim = 1;
}

static void M_Initialise(const int16_t item_num)
{
    BOAT_INFO *boat_data = GameBuf_Alloc(sizeof(BOAT_INFO), GBUF_ITEM_DATA);
    boat_data->boat_turn = 0;
    boat_data->left_fallspeed = 0;
    boat_data->right_fallspeed = 0;
    boat_data->tilt_angle = 0;
    boat_data->extra_rotation = 0;
    boat_data->water = 0;
    boat_data->pitch = 0;

    ITEM *const boat = Item_Get(item_num);
    boat->data = boat_data;
}

static void M_Collision(
    const int16_t item_num, ITEM *const lara, COLL_INFO *const coll)
{
    if (lara->hit_points < 0 || g_Lara.skidoo != NO_ITEM) {
        return;
    }

    const int32_t get_on = M_CheckGetOn(item_num, coll);
    if (!get_on) {
        coll->enable_baddie_push = 1;
        Object_Collision(item_num, lara, coll);
        return;
    }

    g_Lara.skidoo = item_num;

    int16_t boat_anim_idx;
    switch (get_on) {
    case 1:
        boat_anim_idx = BOAT_GET_ON_RW_ANIM;
        break;
    case 2:
        boat_anim_idx = BOAT_GET_ON_LW_ANIM;
        break;
    case 3:
        boat_anim_idx = BOAT_GET_ON_J_ANIM;
        break;
    default:
        boat_anim_idx = BOAT_GET_ON_START;
        break;
    }

    Item_SwitchToObjAnim(lara, boat_anim_idx, 0, O_LARA_BOAT);

    g_Lara.water_status = LWS_ABOVE_WATER;
    g_Lara.hit_direction = -1;

    ITEM *const boat = Item_Get(item_num);

    lara->pos.x = boat->pos.x;
    lara->pos.y = boat->pos.y - 5;
    lara->pos.z = boat->pos.z;
    lara->gravity = 0;
    lara->rot.x = 0;
    lara->rot.y = boat->rot.y;
    lara->rot.z = 0;
    lara->speed = 0;
    lara->fall_speed = 0;
    lara->goal_anim_state = 0;
    lara->current_anim_state = 0;

    if (lara->room_num != boat->room_num) {
        Item_NewRoom(g_Lara.item_num, boat->room_num);
    }

    Item_Animate(lara);
    if (boat->status != IS_ACTIVE) {
        Item_AddActive(item_num);
        boat->status = IS_ACTIVE;
    }
}

static void M_Control(const int16_t item_num)
{
    ITEM *const lara = g_LaraItem;
    ITEM *const boat = Item_Get(item_num);
    BOAT_INFO *const boat_data = (BOAT_INFO *)boat->data;

    bool drive = false;
    int32_t no_turn = 1;
    int32_t collide = M_Dynamics(item_num);

    XYZ_32 fl;
    XYZ_32 fr;
    const int32_t hfl = M_TestWaterHeight(boat, BOAT_FRONT, -BOAT_SIDE, &fl);
    const int32_t hfr = M_TestWaterHeight(boat, BOAT_FRONT, BOAT_SIDE, &fr);

    int16_t room_num = boat->room_num;
    const SECTOR *sector =
        Room_GetSector(boat->pos.x, boat->pos.y, boat->pos.z, &room_num);
    int32_t height =
        Room_GetHeight(sector, boat->pos.x, boat->pos.y, boat->pos.z);
    const int32_t ceiling =
        Room_GetCeiling(sector, boat->pos.x, boat->pos.y, boat->pos.z);

    const int32_t water_height =
        Room_GetWaterHeight(boat->pos.x, boat->pos.y, boat->pos.z, room_num);
    boat_data->water = water_height;

    if (g_Lara.skidoo == item_num && lara->hit_points > 0) {
        switch (lara->current_anim_state) {
        case BOAT_STATE_GET_ON:
        case BOAT_STATE_JUMP_R:
        case BOAT_STATE_JUMP_L:
            break;

        default:
            drive = true;
            no_turn = M_UserControl(boat);
            break;
        }
    } else if (boat->speed > BOAT_SLOWDOWN) {
        boat->speed -= BOAT_SLOWDOWN;
    } else {
        boat->speed = 0;
    }

    if (no_turn) {
        if (boat_data->boat_turn < -BOAT_UNDO_TURN) {
            boat_data->boat_turn += BOAT_UNDO_TURN;
        } else if (boat_data->boat_turn > BOAT_UNDO_TURN) {
            boat_data->boat_turn -= BOAT_UNDO_TURN;
        } else {
            boat_data->boat_turn = 0;
        }
    }

    boat->floor = height - 5;
    if (boat_data->water == NO_HEIGHT) {
        boat_data->water = height;
    } else {
        boat_data->water -= 5;
    }

    boat_data->left_fallspeed =
        M_DoDynamics(hfl, boat_data->left_fallspeed, &fl.y);
    boat_data->right_fallspeed =
        M_DoDynamics(hfr, boat_data->right_fallspeed, &fr.y);
    boat->fall_speed =
        M_DoDynamics(boat_data->water, boat->fall_speed, &boat->pos.y);

    height = (fr.y + fl.y) / 2;

    const int16_t x_rot = Math_Atan(BOAT_FRONT, boat->pos.y - height);
    const int16_t z_rot = Math_Atan(BOAT_SIDE, height - fl.y);
    boat->rot.x += (x_rot - boat->rot.x) / 2;
    boat->rot.z += (z_rot - boat->rot.z) / 2;

    if (x_rot == 0 && ABS(boat->rot.x) < 4) {
        boat->rot.x = 0;
    }
    if (z_rot == 0 && ABS(boat->rot.z) < 4) {
        boat->rot.z = 0;
    }

    if (g_Lara.skidoo == item_num) {
        M_Animation(boat, collide);

        if (room_num != boat->room_num) {
            Item_NewRoom(item_num, room_num);
        }

        boat->rot.z += boat_data->tilt_angle;
        lara->pos.x = boat->pos.x;
        lara->pos.y = boat->pos.y;
        lara->pos.z = boat->pos.z;
        lara->rot.x = boat->rot.x;
        lara->rot.y = boat->rot.y;
        lara->rot.z = boat->rot.z;
        Room_TestTriggers(lara);
        Room_TestTriggers(boat);

        sector = Room_GetSector(
            lara->pos.x, lara->pos.y - 5, lara->pos.z, &room_num);
        if (room_num != g_LaraItem->room_num) {
            Item_NewRoom(g_Lara.item_num, room_num);
        }

        Item_Animate(lara);

        if (lara->hit_points > 0) {
            const int16_t lara_anim_num =
                Item_GetRelativeObjAnim(g_LaraItem, O_LARA_BOAT);
            const int16_t lara_frame_num = Item_GetRelativeFrame(g_LaraItem);
            Item_SwitchToAnim(boat, lara_anim_num, lara_frame_num);
        }

        g_Camera.target_elevation = -20 * DEG_1;
        g_Camera.target_distance = 2 * WALL_L;
    } else {
        if (room_num != boat->room_num) {
            Item_NewRoom(item_num, room_num);
        }
        boat->rot.z += boat_data->tilt_angle;
    }

    const int32_t pitch = water_height - ceiling < BOAT_SOUND_CEILING
        ? boat->speed * (water_height - ceiling) / BOAT_SOUND_CEILING
        : boat->speed;

    boat_data->pitch += ((pitch - boat_data->pitch) >> 2);
    if (boat->speed != 0 && water_height - 5 != boat->pos.y) {
        Sound_Effect(SFX_BOAT_ENGINE, &boat->pos, SPM_NORMAL);
    } else if (boat->speed > 20) {
        Sound_Effect(
            SFX_BOAT_MOVING, &boat->pos,
            PITCH_SHIFT
                + ((0x10000 - (BOAT_MAX_SPEED - boat_data->pitch) * 100) << 8));

    } else if (drive) {
        Sound_Effect(
            SFX_BOAT_IDLE, &boat->pos,
            PITCH_SHIFT
                + ((0x10000 - (BOAT_MAX_SPEED - boat_data->pitch) * 100) << 8));
    }

    if (boat->speed && water_height - 5 == boat->pos.y) {
        M_DoWakeEffect(boat);
    }

    if (g_Lara.skidoo != item_num) {
        return;
    }

    if ((lara->current_anim_state == BOAT_STATE_JUMP_R
         || lara->current_anim_state == BOAT_STATE_JUMP_L)
        && Item_TestFrameEqual(lara, LF_BOAT_EXIT_END)) {
        if (lara->current_anim_state == BOAT_STATE_JUMP_L) {
            lara->rot.y -= DEG_90;
        } else {
            lara->rot.y += DEG_90;
        }

        Item_SwitchToAnim(lara, LA_JUMP_FORWARD, 0);
        lara->goal_anim_state = LS_FORWARD_JUMP;
        lara->current_anim_state = LS_FORWARD_JUMP;
        lara->gravity = 1;
        lara->rot.x = 0;
        lara->rot.z = 0;
        lara->speed = 20;
        lara->fall_speed = -40;
        g_Lara.skidoo = NO_ITEM;

        const XYZ_32 pos = {
            .x = lara->pos.x + ((360 * Math_Sin(lara->rot.y)) >> W2V_SHIFT),
            .y = lara->pos.y - 90,
            .z = lara->pos.z + ((360 * Math_Cos(lara->rot.y)) >> W2V_SHIFT),
        };

        int16_t room_num = lara->room_num;
        const SECTOR *const sector =
            Room_GetSector(pos.x, pos.y, pos.z, &room_num);
        if (Room_GetHeight(sector, pos.x, pos.y, pos.z) >= pos.y - STEP_L) {
            lara->pos.x = pos.x;
            lara->pos.z = pos.z;
            if (room_num != lara->room_num) {
                Item_NewRoom(g_Lara.item_num, room_num);
            }
        }

        lara->pos.y = pos.y;
        Item_SwitchToAnim(boat, 0, 0);
    }
}

REGISTER_OBJECT(O_BOAT, M_Setup)
