#include "game/items.h"
#include "game/lara/common.h"
#include "game/objects/common.h"
#include "game/random.h"
#include "game/room.h"
#include "game/spawn.h"
#include "global/const.h"
#include "global/vars.h"

#define PENDULUM_DAMAGE 100

static void M_Setup(OBJECT *obj);
static void M_Control(int16_t item_num);

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = M_Control;
    obj->collision_func = Object_CollisionTrap;
    obj->shadow_size = UNIT_SHADOW / 2;
    obj->save_flags = 1;
    obj->save_anim = 1;
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);

    if (Item_IsTriggerActive(item)) {
        if (item->current_anim_state == TRAP_SET) {
            item->goal_anim_state = TRAP_WORKING;
        }
    } else {
        if (item->current_anim_state == TRAP_WORKING) {
            item->goal_anim_state = TRAP_SET;
        }
    }

    if (item->current_anim_state == TRAP_WORKING && item->touch_bits) {
        Lara_TakeDamage(PENDULUM_DAMAGE, true);
        int32_t x = g_LaraItem->pos.x + (Random_GetControl() - 0x4000) / 256;
        int32_t z = g_LaraItem->pos.z + (Random_GetControl() - 0x4000) / 256;
        int32_t y = g_LaraItem->pos.y - Random_GetControl() / 44;
        int32_t d = g_LaraItem->rot.y + (Random_GetControl() - 0x4000) / 8;
        Spawn_Blood(x, y, z, g_LaraItem->speed, d, g_LaraItem->room_num);
    }

    const SECTOR *const sector =
        Room_GetSector(item->pos.x, item->pos.y, item->pos.z, &item->room_num);
    item->floor = Room_GetHeight(sector, item->pos.x, item->pos.y, item->pos.z);

    Item_Animate(item);
}

REGISTER_OBJECT(O_PENDULUM, M_Setup)
