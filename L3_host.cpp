#include <string.h>
#include <stdio.h>
#include <vector>
#include <stdlib.h>  
#include <time.h>    
#include <algorithm>    
#include <random>      
#include <ctime> 
#include <iostream>
using namespace std;

#include "L3_host.h"
#include "L3_FSMevent.h"    
#include "L3_timer.h"       
#include "L3_LLinterface.h" 
#include "mbed.h"

// 플레이어 배열
Player players[NUM_PLAYERS];

// 플레이어 ID 리스트 (수동 설정)
uint8_t playerIds[NUM_PLAYERS] = {2, 3, 7, 8};

void shuffleRoles(Role* roles, int size) {
    random_device rd;
    mt19937 g(rd());
    shuffle(roles, roles + size, g);
}

void initPlayer(Player* p, uint8_t id, Role role) {
    p->id = id;
    p->role = role;
    p->isAlive = true;
    p->Voted = 0;
    p->sentVoteId = -1;
    p->policeCheckedId = -1;
}

void createPlayers() {
    srand(time(NULL));

    Role availableRoles[NUM_ROLES];
    for (int i = 0; i < NUM_ROLES; i++) {
        availableRoles[i] = (Role)i;
    }

    shuffleRoles(availableRoles, NUM_ROLES);

    for (int i = 0; i < NUM_PLAYERS; i++) {
        initPlayer(&players[i], playerIds[i], availableRoles[i]);
    }
}

const char* getRoleName(Role r) {
    switch (r) {
        case ROLE_CITIZEN: return "Citizen";
        case ROLE_MAFIA: return "Mafia";
        case ROLE_POLICE: return "Police";
        case ROLE_DOCTOR: return "Doctor";
        default: return "Unknown";
    }
}

int getPlayerIndexById(int id) {
    for (int i = 0; i < NUM_PLAYERS; i++) {
        if (players[i].id == id) return i;
    }
    return -1;
}
