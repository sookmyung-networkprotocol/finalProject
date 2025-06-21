// L3_shared.cpp
#include "L3_shared.h"
#include "mbed.h"
#include <cstring>

// 상태 관리
L3State main_state = L3STATE_IDLE;
L3State prev_state = L3STATE_IDLE;

// 플레이어 정보
Player players[NUM_PLAYERS];
bool dead[NUM_PLAYERS] = {false};
char myRoleName[16] = {0};
bool idead = false;
int doctorTarget = -1;
int killedId = -1;

// 시스템 ID
uint8_t myId = 0;
uint8_t myDestId = 0;

// 메시지 버퍼
char msgStr[20] = {0};
uint8_t sdu[1030] = {0};
uint8_t originalWord[1030] = {0};
uint8_t wordLen = 0;

// 상태 전이 변수
int change_state = 0;

// 시리얼 포트 객체
Serial pc(USBTX, USBRX);

// 게임 종료 판단 함수
bool checkGameOver() {
    int mafiaAlive = 0;
    int citizenAlive = 0;
    for (int i = 0; i < NUM_PLAYERS; i++) {
        if (!players[i].isAlive) continue;
        if (players[i].role == ROLE_MAFIA) mafiaAlive++;
        else citizenAlive++;
    }
    return (mafiaAlive == 0 || mafiaAlive >= citizenAlive);
}

// 게임 상태 초기화 함수
void resetGameState() {
    for (int i = 0; i < NUM_PLAYERS; i++) {
        players[i].isAlive = true;
        players[i].Voted = 0;
        players[i].sentVoteId = -1;
    }
    strcpy(myRoleName, "");
    idead = false;
    doctorTarget = -1;
    killedId = -1;

    wordLen = 0;
    memset(msgStr, 0, sizeof(msgStr));
    memset(sdu, 0, sizeof(sdu));
    memset(originalWord, 0, sizeof(originalWord));

    change_state = 0;
    main_state = MATCH;
    prev_state = L3STATE_IDLE;
    pc.printf("[SYSTEM] 게임 상태가 초기화되었습니다.\n");
}
