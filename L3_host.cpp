#include <cstdlib>
#include <ctime>

#include "L3_shared.h"
#include "L3_host.h"

// NOTE: 'players'는 L3_shared.cpp에서 정의됨
extern Player players[NUM_PLAYERS];

// 각 플레이어에게 할당할 ID 목록
static uint8_t playerIds[NUM_PLAYERS] = {2, 3, 7, 8};

// 플레이어 초기화
void initPlayer(Player* p, uint8_t id, Role role) {
    p->id = id;
    p->role = role;
    p->isAlive = true;
    p->Voted = 0;
    p->sentVoteId = -1;
    memset(p->receivedRole, 0, sizeof(p->receivedRole));
}

// 역할 순서 섞기 (Fisher–Yates shuffle)
void shuffleRoles(Role* roles, int n) {
    for (int i = n - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        Role temp = roles[i];
        roles[i] = roles[j];
        roles[j] = temp;
    }
}

// 플레이어 생성 및 랜덤 역할 배정
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

// 역할 이름을 문자열로 반환
const char* getRoleName(Role r) {
    switch (r) {
        case ROLE_CITIZEN: return "Citizen";
        case ROLE_MAFIA:   return "Mafia";
        case ROLE_POLICE:  return "Police";
        case ROLE_DOCTOR:  return "Doctor";
        default:           return "Unknown";
    }
}
