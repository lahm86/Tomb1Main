#pragma once

#include "global/types.h"

#include <libtrx/game/level.h>

#include <stdbool.h>
#include <stdint.h>

void Level_Load(int32_t level_num);
bool Level_Initialise(int32_t level_num);
LEVEL_INFO *Level_GetInfo(void);
