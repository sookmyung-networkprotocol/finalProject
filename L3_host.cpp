#include <time.h>
#include <cstdlib>   // rand(), srand()
#include <ctime> 

#include "L3_host.h"


Player players[NUM_PLAYERS];
static uint8_t playerIds[NUM_PLAYERS] = {2, 7, 8};  // ID 3(시민) 제거

// 미리 정의된 역할 배정 (ID별로 고정)
// 2: Police, 7: Mafia, 8: Doctor (시민 제외)
static Role predefinedRoles[NUM_PLAYERS] = {ROLE_POLICE, ROLE_MAFIA, ROLE_DOCTOR};

// 플레이어 초기화 함수
void initPlayer(Player* p, uint8_t id, Role role) {
    p->id = id;
    p->role = role;
    p->isAlive = true;
    p->Voted = 0;
    p->sentVoteId = -1; 
}

// 기존 랜덤 역할 배정 함수는 더 이상 사용하지 않음
void shuffleRoles(Role *roles, int n) {
    // 이 함수는 더 이상 사용되지 않지만 호환성을 위해 남겨둠
    for (int i = n - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        Role temp = roles[i];
        roles[i] = roles[j];
        roles[j] = temp;
    }
}

void createPlayers() {
    // srand는 더 이상 필요하지 않지만 다른 용도로 사용될 수 있으므로 남겨둠
    srand(time(NULL));

    // 미리 정의된 역할로 플레이어 생성
    for (int i = 0; i < NUM_PLAYERS; i++) {
        initPlayer(&players[i], playerIds[i], predefinedRoles[i]);
        players[i].sentVoteId = -1; 
    }
}

// 역할 문자열 반환 함수 (시민 케이스 제거)
const char* getRoleName(Role r) {
    switch (r) {
        case ROLE_MAFIA:   return "Mafia";
        case ROLE_POLICE:  return "Police";
        case ROLE_DOCTOR:  return "Doctor";
        default:           return "Unknown";
    }
}