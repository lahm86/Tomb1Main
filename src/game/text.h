#ifndef TR1M_GAME_TEXT_H
#define TR1M_GAME_TEXT_H

#include "game/types.h"

// clang-format off
#define T_DrawText              ((void          __cdecl(*)())0x00439B00)
#define T_InitPrint             ((void          __cdecl(*)())0x00439750)
#define T_Print                 ((TEXTSTRING*   __cdecl(*)(int16_t xpos, int16_t ypos, int16_t zpos, const char *string))0x00439780)
#define T_RemovePrint           ((void          __cdecl(*)(TEXTSTRING *text_string))0x00439AD0)
#define T_RightAlign            ((void          __cdecl(*)(TEXTSTRING *text_string, int16_t enable))0x004399E0)
#define T_ChangeText            ((void          __cdecl(*)(TEXTSTRING *text_string, const char *string))0x00439860)
// clang-format on

void __cdecl T_CentreH(TEXTSTRING* textstring, int16_t b);
void __cdecl T_CentreV(TEXTSTRING* textstring, int16_t b);

void TR1MInjectText();

#endif
