#include "L3_shared.h"

int change_state = 0;
bool dead[NUM_PLAYERS] = { false };
char myRoleName[16] = {0};
bool idead = false;
int doctorTarget = -1;
Player players[NUM_PLAYERS];
uint8_t myId = 0;
uint8_t myDestId = 0;
