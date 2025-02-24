#include "game/objects/vars.h"

#include <libtrx/game/objects/ids.h>

const GAME_OBJECT_ID g_EnemyObjects[] = {
    // clang-format off
    O_DOG,
    O_CULT_1,
    O_CULT_1A,
    O_CULT_1B,
    O_CULT_2,
    O_CULT_3,
    O_MOUSE,
    O_DRAGON_FRONT,
    O_SHARK,
    O_EEL,
    O_BIG_EEL,
    O_BARRACUDA,
    O_DIVER,
    O_WORKER_1,
    O_WORKER_2,
    O_WORKER_3,
    O_WORKER_4,
    O_WORKER_5,
    O_JELLY,
    O_SPIDER,
    O_BIG_SPIDER,
    O_CROW,
    O_TIGER,
    O_BARTOLI,
    O_XIAN_SPEARMAN,
    O_XIAN_SPEARMAN_STATUE,
    O_XIAN_KNIGHT,
    O_XIAN_KNIGHT_STATUE,
    O_YETI,
    O_BIRD_GUARDIAN,
    O_EAGLE,
    O_BANDIT_1,
    O_BANDIT_2,
    O_BANDIT_2B,
    O_SKIDOO_DRIVER,
    O_DINO,
    NO_OBJECT,
    // clang-format on
};

const GAME_OBJECT_ID g_WaterObjects[] = {
    // clang-format off
    O_SHARK,
    O_EEL,
    O_BIG_EEL,
    O_BARRACUDA,
    O_DIVER,
    O_JELLY,
    O_GENERAL,
    O_PROPELLER_2,
    NO_OBJECT,
    // clang-format on
};

const GAME_OBJECT_ID g_AllyObjects[] = {
    // clang-format off
    O_LARA,
    O_WINSTON,
    O_MONK_1,
    O_MONK_2,
    O_DYING_MONK,
    NO_OBJECT,
    // Lara's social skills: still loading...
    // clang-format on
};

const GAME_OBJECT_ID g_PickupObjects[] = {
    // clang-format off
    O_PISTOL_ITEM,
    O_SHOTGUN_ITEM,
    O_MAGNUM_ITEM,
    O_UZI_ITEM,
    O_HARPOON_ITEM,
    O_M16_ITEM,
    O_GRENADE_ITEM,
    O_PISTOL_AMMO_ITEM,
    O_SHOTGUN_AMMO_ITEM,
    O_MAGNUM_AMMO_ITEM,
    O_UZI_AMMO_ITEM,
    O_HARPOON_AMMO_ITEM,
    O_M16_AMMO_ITEM,
    O_GRENADE_AMMO_ITEM,
    O_SMALL_MEDIPACK_ITEM,
    O_LARGE_MEDIPACK_ITEM,
    O_FLARE_ITEM,
    O_FLARES_ITEM,
    O_PUZZLE_ITEM_1,
    O_PUZZLE_ITEM_2,
    O_PUZZLE_ITEM_3,
    O_PUZZLE_ITEM_4,
    O_KEY_ITEM_1,
    O_KEY_ITEM_2,
    O_KEY_ITEM_3,
    O_KEY_ITEM_4,
    O_PICKUP_ITEM_1,
    O_PICKUP_ITEM_2,
    O_SECRET_1,
    O_SECRET_2,
    O_SECRET_3,
    NO_OBJECT,
    // clang-format on
};

const GAME_OBJECT_ID g_DoorObjects[] = {
    // clang-format off
    O_DOOR_TYPE_1,
    O_DOOR_TYPE_2,
    O_DOOR_TYPE_3,
    O_DOOR_TYPE_4,
    O_DOOR_TYPE_5,
    O_DOOR_TYPE_6,
    O_DOOR_TYPE_7,
    O_DOOR_TYPE_8,
    NO_OBJECT,
    // clang-format on
};

const GAME_OBJECT_ID g_TrapdoorObjects[] = {
    // clang-format off
    O_TRAPDOOR_TYPE_1,
    O_TRAPDOOR_TYPE_2,
    O_TRAPDOOR_TYPE_3,
    O_DRAWBRIDGE,
    NO_OBJECT,
    // clang-format on
};

const GAME_OBJECT_ID g_AnimObjects[] = {
    // clang-format off
    O_LARA_PISTOLS,
    O_LARA_HAIR,
    O_LARA_SHOTGUN,
    O_LARA_MAGNUMS,
    O_LARA_UZIS,
    O_LARA_M16,
    O_LARA_GRENADE,
    O_LARA_HARPOON,
    O_LARA_FLARE,
    O_LARA_SKIDOO,
    O_LARA_BOAT,
    O_LARA_EXTRA,
    // clang-format on
};

const GAME_OBJECT_ID g_NullObjects[] = {
    // clang-format off
    O_WATER_SPRITE,
    O_SNOW_SPRITE,
    O_TEXT_BOX,
    O_FLARE_ITEM,
    O_SPHERE_OF_DOOM_1,
    O_SPHERE_OF_DOOM_2,
    O_SPHERE_OF_DOOM_3,
    O_DRAGON_BONES_2,
    O_DRAGON_BONES_3,
    O_HOT_LIQUID,
    O_INV_BACKGROUND,
    O_FX_RESERVED,
    O_GONG_BONGER,
    O_EXPLOSION,
    O_SPLASH,
    O_BUBBLE,
    O_BUBBLE_EMITTER,
    O_BLOOD,
    O_DART_EFFECT,
    O_FLARE_FIRE,
    O_GLOW,
    O_GLOW_RESERVED,
    O_RICOCHET,
    O_TWINKLE,
    O_GUN_FLASH,
    O_M16_FLASH,
    O_MISSILE_HARPOON,
    O_MISSILE_FLAME,
    O_MISSILE_KNIFE,
    O_GRENADE,
    O_HARPOON_BOLT,
    O_SKYBOX,
    O_ALPHABET,
    O_ASSAULT_DIGITS,
    O_FINAL_LEVEL_COUNTER,
    O_CUT_SHOTGUN,
    O_EARTHQUAKE,
    // clang-format on
};

const GAME_OBJECT_ID g_InvObjects[] = {
    // clang-format off
    O_PISTOL_OPTION,
    O_SHOTGUN_OPTION,
    O_MAGNUM_OPTION,
    O_UZI_OPTION,
    O_HARPOON_OPTION,
    O_M16_OPTION,
    O_GRENADE_OPTION,
    O_PISTOL_AMMO_OPTION,
    O_SHOTGUN_AMMO_OPTION,
    O_MAGNUM_AMMO_OPTION,
    O_UZI_AMMO_OPTION,
    O_HARPOON_AMMO_OPTION,
    O_M16_AMMO_OPTION,
    O_GRENADE_AMMO_OPTION,
    O_SMALL_MEDIPACK_OPTION,
    O_LARGE_MEDIPACK_OPTION,
    O_FLARES_OPTION,
    O_PUZZLE_OPTION_1,
    O_PUZZLE_OPTION_2,
    O_PUZZLE_OPTION_3,
    O_PUZZLE_OPTION_4,
    O_KEY_OPTION_1,
    O_KEY_OPTION_2,
    O_KEY_OPTION_3,
    O_KEY_OPTION_4,
    O_PICKUP_OPTION_1,
    O_PICKUP_OPTION_2,
    O_DETAIL_OPTION,
    O_SOUND_OPTION,
    O_CONTROL_OPTION,
    O_GAMMA_OPTION,
    O_PASSPORT_OPTION,
    O_COMPASS_OPTION,
    O_PHOTO_OPTION,
    NO_OBJECT,
    // clang-format on
};

const GAME_OBJECT_PAIR g_ItemToInvObjectMap[] = {
    // clang-format off
    { O_COMPASS_ITEM, O_COMPASS_OPTION },
    { O_PISTOL_ITEM, O_PISTOL_OPTION },
    { O_SHOTGUN_ITEM, O_SHOTGUN_OPTION },
    { O_MAGNUM_ITEM, O_MAGNUM_OPTION },
    { O_UZI_ITEM, O_UZI_OPTION },
    { O_HARPOON_ITEM, O_HARPOON_OPTION },
    { O_M16_ITEM, O_M16_OPTION },
    { O_GRENADE_ITEM, O_GRENADE_OPTION },
    { O_PISTOL_AMMO_ITEM, O_PISTOL_AMMO_OPTION },
    { O_SHOTGUN_AMMO_ITEM, O_SHOTGUN_AMMO_OPTION },
    { O_MAGNUM_AMMO_ITEM, O_MAGNUM_AMMO_OPTION },
    { O_UZI_AMMO_ITEM, O_UZI_AMMO_OPTION },
    { O_HARPOON_AMMO_ITEM,O_HARPOON_AMMO_OPTION },
    { O_M16_AMMO_ITEM, O_M16_AMMO_OPTION },
    { O_GRENADE_AMMO_ITEM, O_GRENADE_AMMO_OPTION },
    { O_SMALL_MEDIPACK_ITEM, O_SMALL_MEDIPACK_OPTION },
    { O_LARGE_MEDIPACK_ITEM, O_LARGE_MEDIPACK_OPTION },
    { O_FLARE_ITEM, O_FLARES_OPTION },
    { O_FLARES_ITEM, O_FLARES_OPTION },
    { O_PUZZLE_ITEM_1, O_PUZZLE_OPTION_1 },
    { O_PUZZLE_ITEM_2, O_PUZZLE_OPTION_2 },
    { O_PUZZLE_ITEM_3, O_PUZZLE_OPTION_3 },
    { O_PUZZLE_ITEM_4, O_PUZZLE_OPTION_4 },
    { O_KEY_ITEM_1, O_KEY_OPTION_1 },
    { O_KEY_ITEM_2, O_KEY_OPTION_2 },
    { O_KEY_ITEM_3, O_KEY_OPTION_3 },
    { O_KEY_ITEM_4, O_KEY_OPTION_4 },
    { O_PICKUP_ITEM_1, O_PICKUP_OPTION_1 },
    { O_PICKUP_ITEM_2, O_PICKUP_OPTION_2 },
    { NO_OBJECT, NO_OBJECT },
    // clang-format on
};
