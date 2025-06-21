#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "L3_host.h"

#define NUM_PLAYERS 4

// 상태 열거형 정의
typedef enum {
    L3STATE_IDLE = 0,
    MATCH,
    MODE_1,
    DAY,
    VOTE,
    NIGHT,
    MODE_2,
    MAFIA,
    DOCTOR,
    POLICE,
    OVER,
    TYPING
} L3State;

// 공통 FSM 상태 변수
extern L3State main_state;

// 플레이어 상태 및 역할
extern Player players[NUM_PLAYERS];
extern bool dead[NUM_PLAYERS];         // deprecated 가능
extern char myRoleName[16];
extern bool idead;
extern int doctorTarget;

// 시스템 ID
extern uint8_t myId;
extern uint8_t myDestId;

// 키보드 입력/버퍼 처리
extern char msgStr[20];
extern uint8_t sdu[1030];
extern uint8_t originalWord[1030];
extern uint8_t wordLen;

// 상태 전이용 임시 변수
extern int change_state;

// 게임 종료 판단 함수
bool checkGameOver();
