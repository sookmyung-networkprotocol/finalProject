#include "L3_shared.h"
#include "L3_host.h"
#include <cstring>
#include <cstdio>

// -------------------- STATE VARIABLES DEFINITION --------------------
Player players[NUM_PLAYERS];

L3State main_state = L3STATE_IDLE;
L3State prev_state = L3STATE_IDLE;

uint8_t myId = 0;
uint8_t myDestId = 0;

char myRoleName[16] = {0};
bool idead = false;
int doctorTarget = -1;
int killedId = -1;

int change_state = 0;

char msgStr[20] = {0};
uint8_t originalWord[1030] = {0};
uint8_t sdu[1030] = {0};
uint8_t wordLen = 0;

// Serial 포트 (Nucleo 보드와 연결)
Serial pc(USBTX, USBRX);

// -------------------- FUNCTIONS --------------------
bool checkGameOver() {
    int mafiaAlive = 0;
    int othersAlive = 0;

    for (int i = 0; i < NUM_PLAYERS; i++) {
        if (players[i].isAlive) {
            if (players[i].role == ROLE_MAFIA)
                mafiaAlive++;
            else
                othersAlive++;
        }
    }

    return (mafiaAlive == 0 || mafiaAlive >= othersAlive);
}

void resetGameState() {
    // 1. 플레이어 재생성 (랜덤 역할 부여 포함)
    createPlayers();

    // 2. FSM 상태 초기화
    main_state = L3STATE_IDLE;
    prev_state = L3STATE_IDLE;
    change_state = 0;

    // 3. 통신 버퍼 초기화
    wordLen = 0;
    memset(originalWord, 0, sizeof(originalWord));
    memset(sdu, 0, sizeof(sdu));
    memset(msgStr, 0, sizeof(msgStr));
    doctorTarget = -1;
    killedId = -1;
    idead = false;

    pc.printf("\n[RESET] 게임 상태가 초기화되었습니다. 새로운 게임을 시작합니다.\n");
}
