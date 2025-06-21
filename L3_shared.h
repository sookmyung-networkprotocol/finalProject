#pragma once

#include <stdint.h>
#include <stdbool.h>
#define L3_MAXDATASIZE 1030

// -------------------- ENUM --------------------
typedef enum {
    ROLE_CITIZEN,
    ROLE_MAFIA,
    ROLE_POLICE,
    ROLE_DOCTOR
} Role;

typedef enum {
    L3STATE_IDLE = 0,
    MATCH,
    MODE_1,
    DAY,
    VOTE,
    NIGHT,
    DOCTOR,
    POLICE,
    MODE_2,
    OVER,
    TYPING
} L3State;

// -------------------- PLAYER STRUCT --------------------
#define NUM_PLAYERS 4
#define NUM_ROLES 4

typedef struct {
    uint8_t id;              // 플레이어 ID
    Role role;               // 역할
    bool isAlive;            // 생존 여부
    int8_t Voted;            // 받은 표 수
    int8_t sentVoteId;       // 내가 투표한 대상 ID
    char receivedRole[16];   // 문자열 형태의 역할 정보 저장
} Player;

// -------------------- SHARED VARIABLES --------------------
extern Player players[NUM_PLAYERS];

extern L3State main_state;
extern L3State prev_state;

extern uint8_t myId;
extern uint8_t myDestId;

extern char myRoleName[16];
extern bool idead;
extern int doctorTarget;
extern int killedId;

extern int change_state;

extern char msgStr[20];
extern uint8_t originalWord[1030];
extern uint8_t sdu[1030];
extern uint8_t wordLen;

// 시리얼 포트 인터페이스
#include "mbed.h"
extern Serial pc;

// -------------------- FUNCTIONS --------------------
bool checkGameOver();
void resetGameState();
