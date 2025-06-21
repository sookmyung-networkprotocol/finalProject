#include "L3_state_over.h"
#include "L3_shared.h"
#include "L3_LLinterface.h"
#include "L3_FSMevent.h"
#include "L2_msg.h"
#include "L3_host.h"
#include <cstring>
#include <cstdio>

void L3_handleMode2() {
    static bool allSent = false;

    if (!allSent) {
        printf("[HOST] Sending MODE:OVER to all players...\n");

        for (int i = 0; i < NUM_PLAYERS; i++) {
            if (players[i].id != myId && players[i].isAlive) {
                L3_LLI_dataReqFunc((uint8_t*)"MODE:OVER", strlen("MODE:OVER"), players[i].id);
            }
        }

        allSent = true;
    }

    if (L3_event_checkEventFlag(L3_event_msgRcvd)) {
        uint8_t* msg = L3_LLI_getMsgPtr();
        if (L2_msg_checkIfAck(msg)) {
            printf("[HOST] ACK received from player.\n");
        }
        L3_event_clearEventFlag(L3_event_msgRcvd);
    }

    main_state = OVER;
}

void L3_handleOver() {
    printf("\n===== GAME OVER =====\n");

    for (int i = 0; i < NUM_PLAYERS; i++) {
        printf("Player %d (ID: %d) - Role: %s - Status: %s\n",
               i, players[i].id,
               getRoleName(players[i].role),
               players[i].isAlive ? "Alive" : "Dead");
    }

    printf("======================\n\n");

    if (myId == 1) {
        printf("게임을 다시 시작하려면 y 를 입력하세요: ");

        if (pc.readable()) {
            char c = pc.getc();
            if (c == 'y' || c == 'Y') {
                resetGameState();  // 모든 FSM 상태와 변수 초기화
            }
        }
    }
}
