#pragma once

#include <stdint.h>
#include <stdbool.h>

// 역할 정의
typedef enum {
    ROLE_CITIZEN,
    ROLE_MAFIA,
    ROLE_POLICE,
    ROLE_DOCTOR
} Role;

// 플레이어 구조체 정의
typedef struct {
    uint8_t id;         // 플레이어 ID
    Role role;          // 역할
    bool isAlive;       // 생존 여부
    int8_t Voted;       // 받은 표 수
    int8_t sentVoteId;  // 내가 투표한 대상 ID
} Player;

#define NUM_PLAYERS 4
#define NUM_ROLES 4

// 전역 플레이어 배열 참조
extern Player players[NUM_PLAYERS];

// 함수 선언
void initPlayer(Player* p, uint8_t id, Role role);
void createPlayers();
const char* getRoleName(Role r);
