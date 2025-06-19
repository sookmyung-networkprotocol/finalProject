#include <time.h>
#include <cstdlib>   // rand(), srand()
#include <ctime> 

#include "L3_host.h"


Player players[NUM_PLAYERS];
static uint8_t playerIds[NUM_PLAYERS] = {2, 3, 7, 8};

// 플레이어 초기화 함수
void initPlayer(Player* p, uint8_t id, Role role) {
    p->id = id;
    p->role = role;
    p->isAlive = true;
    p->Voted = 0;
    p->sentVoteId = -1;  // ✅ 초기값 설정 (투표 안 했음을 의미)

}

// 랜덤 역할 배정 
void shuffleRoles(Role *roles, int n) {
    for (int i = n - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        Role temp = roles[i];
        roles[i] = roles[j];
        roles[j] = temp;
    }
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

// 역할 문자열 반환 함수
const char* getRoleName(Role r) {
    switch (r) {
        case ROLE_CITIZEN: return "Citizen";
        case ROLE_MAFIA:   return "Mafia";
        case ROLE_POLICE:  return "Police";
        case ROLE_DOCTOR:  return "Doctor";
        default:           return "Unknown";
    }
}