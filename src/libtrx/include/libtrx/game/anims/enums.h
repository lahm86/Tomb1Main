#pragma once

typedef enum {
// TODO: merge
#if TR_VERSION == 1
    BEB_POP = 1 << 0,
    BEB_PUSH = 1 << 1,
    BEB_ROT_X = 1 << 2,
    BEB_ROT_Y = 1 << 3,
    BEB_ROT_Z = 1 << 4,
#else
    BF_MATRIX_POP = 1,
    BF_MATRIX_PUSH = 2,
    BF_ROT_X = 4,
    BF_ROT_Y = 8,
    BF_ROT_Z = 16,
#endif
} BONE_FLAGS;
