#include "game/objects/traps/springboard.h"

#include "game/items.h"
#include "game/objects/common.h"
#include "game/spawn.h"
#include "global/utils.h"
#include "global/vars.h"

#include <libtrx/game/lara/common.h>

typedef enum {
    // clang-format off
    SPRINGBOARD_STATE_OFF = 0,
    SPRINGBOARD_STATE_ON = 1,
    // clang-format on
} SPRINGBOARD_STATE;

void __cdecl Springboard_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    ITEM *const lara_item = Lara_GetItem();

    if (item->current_anim_state == SPRINGBOARD_STATE_OFF
        && lara_item->pos.y == item->pos.y
        && ROUND_TO_SECTOR(lara_item->pos.x) == ROUND_TO_SECTOR(item->pos.x)
        && ROUND_TO_SECTOR(lara_item->pos.z) == ROUND_TO_SECTOR(item->pos.z)) {
        if (lara_item->hit_points <= 0) {
            return;
        }

        if (lara_item->current_anim_state == LS_BACK
            || lara_item->current_anim_state == LS_FAST_BACK) {
            lara_item->speed = -lara_item->speed;
        }

        lara_item->fall_speed = -240;
        lara_item->gravity = 1;

        lara_item->anim_num = LA_FALL_START;
        lara_item->frame_num = g_Anims[lara_item->anim_num].frame_base;
        lara_item->current_anim_state = LS_FORWARD_JUMP;
        lara_item->goal_anim_state = LS_FORWARD_JUMP;
        item->goal_anim_state = SPRINGBOARD_STATE_ON;
    }

    Item_Animate(item);
}

void Springboard_Setup(void)
{
    OBJECT *const obj = Object_GetObject(O_SPRINGBOARD);
    obj->control = Springboard_Control;
    obj->save_flags = 1;
    obj->save_anim = 1;
}
