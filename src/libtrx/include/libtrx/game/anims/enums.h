#pragma once

typedef enum {
    BF_MATRIX_POP = 1,
    BF_MATRIX_PUSH = 2,
    BF_ROT_X = 4,
    BF_ROT_Y = 8,
    BF_ROT_Z = 16,
} BONE_FLAGS;

#if TR_VERSION > 1
typedef enum {
    RPM_ALL = 0,
    RPM_X = 0x4000,
    RPM_Y = 0x8000,
    RPM_Z = 0xC000,
} ROT_PACK_MODE;
#endif
