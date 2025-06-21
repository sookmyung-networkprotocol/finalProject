// L3_state_over.cpp
#include "L3_state_over.h"
#include "L3_shared.h"
#include "L3_LLinterface.h"
#include "L3_FSMevent.h"
#include <cstdio>
#include <cstring>

static int ackCount = 0;
static bool sentToAll = false;

void L3_handleMode2() {
    if (!sentToAll) {
        pc.printf("[MODE2] 게임 종료 메시지를 모든 생존자에게 전송합니다.\n");
        for (int i = 0; i < NUM_PLAYERS; i++) {
            if (players[i].id != myId && players[i].isAlive) {
                const char* msg = "MODE:OVER";
                L3_LLI_dataReqFunc((uint8_t*)msg, strlen(msg), players[i].id);
            }
        }
        sentToAll = true;
    }

    if (L3_event_checkEventFlag(L3_event_msgRcvd)) {
        uint8_t* dataPtr = L3_LLI_getMsgPtr();
        uint8_t fromId = L3_LLI_getSrcId();

        if (strncmp((char*)dataPtr, "ACK", 3) == 0) {
            pc.printf("[MODE2] %d번 플레이어로부터 ACK 수신\n", fromId);
            ackCount++;
        }

        L3_event_clearEventFlag(L3_event_msgRcvd);
    }

    if (ackCount >= NUM_PLAYERS - 1) {
        pc.printf("[MODE2] 모든 ACK 수신 완료. OVER 상태로 전환합니다.\n");
        main_state = OVER;
        sentToAll = false;
        ackCount = 0;
    }
}

void L3_handleOver() {
    pc.printf("\n=========== GAME OVER ===========\n");

    for (int i = 0; i < NUM_PLAYERS; i++) {
        const char* status = players[i].isAlive ? "Alive" : "Dead";
        pc.printf("Player ID: %d | Role: %s | Status: %s\n",
                  players[i].id,
                  getRoleName(players[i].role),
                  status);
    }

    pc.printf("\n게임이 종료되었습니다. 재시작하려면 아무 키나 누르세요...\n");

    // 사용자 입력 대기 후 재시작 처리
    while (!pc.readable());
    char dummy = pc.getc();
    resetGameState();
    main_state = MATCH;
}
