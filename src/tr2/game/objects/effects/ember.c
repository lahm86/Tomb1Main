#include "game/effects.h"
#include "game/lara/control.h"
#include "game/lara/misc.h"
#include "game/room.h"
#include "global/vars.h"

#include <libtrx/game/math.h>

static void M_Setup(OBJECT *obj);
static void M_Control(int16_t effect_num);

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = M_Control;
    obj->semi_transparent = 1;
}

static void M_Control(const int16_t effect_num)
{
    EFFECT *const effect = Effect_Get(effect_num);
    effect->fall_speed += GRAVITY;
    effect->pos.z += (effect->speed * Math_Cos(effect->rot.y)) >> W2V_SHIFT;
    effect->pos.x += (effect->speed * Math_Sin(effect->rot.y)) >> W2V_SHIFT;
    effect->pos.y = effect->pos.y + effect->fall_speed;

    int16_t room_num = effect->room_num;
    const SECTOR *const sector =
        Room_GetSector(effect->pos.x, effect->pos.y, effect->pos.z, &room_num);
    const int32_t ceiling =
        Room_GetCeiling(sector, effect->pos.x, effect->pos.y, effect->pos.z);
    const int32_t height =
        Room_GetHeight(sector, effect->pos.x, effect->pos.y, effect->pos.z);

    if (effect->pos.y >= height || effect->pos.y < ceiling) {
        Effect_Kill(effect_num);
    } else if (Lara_IsNearItem(&effect->pos, 200)) {
        Lara_TakeDamage(10, true);
        Effect_Kill(effect_num);
    } else if (room_num != effect->room_num) {
        Effect_NewRoom(effect_num, room_num);
    }
}

REGISTER_OBJECT(O_EMBER, M_Setup)
