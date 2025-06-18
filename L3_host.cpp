#include <time.h>
#include <cstdlib>   // rand(), srand()
#include <ctime> 

#include "L3_host.h"

#define NUM_PLAYERS 4
#define NUM_ROLES 4

// 플레이어 ID 목록
static uint8_t playerIds[NUM_PLAYERS] = {2, 3, 7, 8};

// 실제 플레이어 배열 정의
Player players[NUM_PLAYERS];

// 플레이어 초기화 함수
void initPlayer(Player* p, uint8_t id, Role role) {
    p->id = id;
    p->role = role;
    p->isAlive = true;
    p->sentVoteId = -1;
}

// 역할 배정 
void createPlayers() {
    srand(time(NULL)); // 한 번만 호출

    for (int i = 0; i < NUM_PLAYERS; i++) {
        Role randomRole = (Role)(rand() % NUM_ROLES);
        initPlayer(&players[i], playerIds[i], randomRole);
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