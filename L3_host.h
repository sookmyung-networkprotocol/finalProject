#pragma once

#include <stdint.h>
#include "L3_shared.h"  // Role, Player 포함

void initPlayer(Player* p, uint8_t id, Role role);
void createPlayers();
const char* getRoleName(Role r);
