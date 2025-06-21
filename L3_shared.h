#pragma once
#include "mbed.h"
#include <stdint.h>
#include <stdbool.h>
#include "L3_host.h"

// ------------------------- 상태 정의 -------------------------
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

// ---------------------- 상태 변수 ----------------------
extern L3State main_state;
extern L3State prev_state;

// ---------------------- 플레이어 상태 ----------------------
extern Player players[NUM_PLAYERS];
extern bool dead[NUM_PLAYERS];         // deprecated 가능
extern char myRoleName[16];
extern bool idead;
extern int doctorTarget;
extern int killedId;

// ---------------------- 시스템 ID ----------------------
extern uint8_t myId;
extern uint8_t myDestId;

// ---------------------- 키보드 입력/버퍼 ----------------------
extern char msgStr[20];
extern uint8_t sdu[1030];
extern uint8_t originalWord[1030];
extern uint8_t wordLen;

// ---------------------- 상태 전이용 플래그 ----------------------
extern int change_state;

// ---------------------- 시리얼 포트 객체 ----------------------
extern Serial pc;

// ---------------------- 유틸 함수 ----------------------
bool checkGameOver();

void resetGameState();