#pragma once

// clang-format off
typedef enum {
    // TODO reset to 1
    INJ_VERSION_1  = 1,
#if TR_VERSION == 1
    INJ_VERSION_2  = 2,
    INJ_VERSION_3  = 3,
    INJ_VERSION_4  = 4,
    INJ_VERSION_5  = 5,
    INJ_VERSION_6  = 6,
    INJ_VERSION_7  = 7,
    INJ_VERSION_8  = 8,
    INJ_VERSION_9  = 9,
    INJ_VERSION_10 = 10,
    INJ_VERSION_11 = 11,
#endif
} INJECTION_VERSION;

typedef enum {
    IMT_GENERAL             = 0,
    IMT_BRAID               = 1,
    IMT_TEXTURE_FIX         = 2,
    IMT_UZI_SFX             = 3,
    IMT_FLOOR_DATA          = 4,
    IMT_LARA_ANIMS          = 5,
    IMT_ITEM_POSITION       = 6,
    IMT_PS1_ENEMY           = 7,
    IMT_DISABLE_ANIM_SPRITE = 8,
    IMT_SKYBOX              = 9,
    IMT_PS1_CRYSTAL         = 10,
    IMT_NUMBER_OF           = 11,
} INJECTION_MAIN_TYPE;

typedef enum {
    IST_TEXTURE_DATA   = 0,
    IST_TEXTURE_INFO   = 1,
    IST_MESH_DATA      = 2,
    IST_ANIMATION_DATA = 3,
    IST_OBJECT_DATA    = 4,
    IST_SFX_DATA       = 5,
    IST_DATA_EDITS     = 6,
} INJECTION_SET_TYPE;

typedef enum {
    IDT_PALETTE          = 0,
    IDT_IMAGES           = 1,
    IDT_OBJECT_TEXTURES  = 2,
    IDT_SPRITE_TEXTURES  = 3,
    IDT_SPRITE_INFOS     = 4,
    IDT_OBJECT_MESHES    = 5,
    IDT_MESH_POINTERS    = 6,
    IDT_ANIM_CHANGES     = 7,
    IDT_ANIM_RANGES      = 8,
    IDT_ANIM_COMMANDS    = 9,
    IDT_ANIM_BONES       = 10,
    IDT_ANIM_FRAMES      = 11,
    IDT_ANIM_FRAME_ROTS  = 12,
    IDT_ANIMS            = 13,
    IDT_OBJECTS          = 14,
    IDT_SAMPLE_INFOS     = 15,
    IDT_SAMPLE_INDICES   = 16,
    IDT_SAMPLE_DATA      = 17,
    IDT_FLOOR_EDITS      = 18,
    IDT_ITEM_EDITS       = 19,
    IDT_MESH_EDITS       = 20,
    IDT_TEXTURE_EDITS    = 21,
    IDT_ROOM_EDIT_META   = 22,
    IDT_ROOM_EDITS       = 23,
    IDT_VIS_PORTAL_EDITS = 24,
    IDT_NUMBER_OF        = 25,
} INJECTION_DATA_TYPE;

typedef enum {
    FT_TEXTURED_QUAD     = 0,
    FT_TEXTURED_TRIANGLE = 1,
    FT_COLOURED_QUAD     = 2,
    FT_COLOURED_TRIANGLE = 3
} FACE_TYPE;

typedef enum {
    FET_TRIGGER_PARAM   = 0,
    FET_MUSIC_ONESHOT   = 1,
    FET_FD_INSERT       = 2,
    FET_ROOM_SHIFT      = 3,
    FET_TRIGGER_ITEM    = 4,
    FET_ROOM_PROPERTIES = 5,
    FET_TRIGGER_TYPE    = 6,
} FLOOR_EDIT_TYPE;

typedef enum {
    RMET_TEXTURE_FACE = 0,
    RMET_MOVE_FACE    = 1,
    RMET_ALTER_VERTEX = 2,
    RMET_ROTATE_FACE  = 3,
    RMET_ADD_FACE     = 4,
    RMET_ADD_VERTEX   = 5,
    RMET_ADD_SPRITE   = 6,
} ROOM_MESH_EDIT_TYPE;
// clang-format on
