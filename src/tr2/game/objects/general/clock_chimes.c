#include "game/items.h"
#include "game/objects/common.h"
#include "game/sound.h"

#include <libtrx/game/lara/common.h>

static void M_DoChimeSound(const ITEM *item);
static void M_Control(int16_t item_num);
static void M_Setup(OBJECT *obj);

static void M_DoChimeSound(const ITEM *const item)
{
    const ITEM *const lara_item = Lara_GetItem();
    XYZ_32 pos = lara_item->pos;
    pos.x += (item->pos.x - lara_item->pos.x) >> 6;
    pos.y += (item->pos.y - lara_item->pos.y) >> 6;
    pos.z += (item->pos.z - lara_item->pos.z) >> 6;
    Sound_Effect(SFX_DOOR_CHIME, &pos, SPM_NORMAL);
}

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = M_Control;
    obj->draw_func = Object_DrawDummyItem;
    obj->save_flags = 1;
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    if (item->timer == 0) {
        return;
    }
    if (item->timer % 60 == 59) {
        M_DoChimeSound(item);
    }
    item->timer--;
    if (item->timer == 0) {
        M_DoChimeSound(item);
        item->timer = -1;
        Item_RemoveActive(item_num);
        item->status = IS_INACTIVE;
        item->flags &= ~IF_CODE_BITS;
    }
}

REGISTER_OBJECT(O_CLOCK_CHIMES, M_Setup)
