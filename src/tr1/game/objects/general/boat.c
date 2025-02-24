#include "game/items.h"

typedef enum {
    BOAT_STATE_EMPTY = 0,
    BOAT_STATE_SET = 1,
    BOAT_STATE_MOVE = 2,
    BOAT_STATE_STOP = 3,
} BOAT_STATE;

static void M_Setup(OBJECT *obj);
static void M_Control(int16_t effect_num);

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = M_Control;
    obj->save_flags = 1;
    obj->save_anim = 1;
    obj->save_position = 1;
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);

    switch (item->current_anim_state) {
    case BOAT_STATE_SET:
        item->goal_anim_state = BOAT_STATE_MOVE;
        break;
    case BOAT_STATE_MOVE:
        item->goal_anim_state = BOAT_STATE_STOP;
        break;
    case BOAT_STATE_STOP:
        Item_Kill(item_num);
        break;
    }

    Item_Animate(item);
}

REGISTER_OBJECT(O_BOAT, M_Setup)
