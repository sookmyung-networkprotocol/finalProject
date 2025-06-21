#include "L3_shared.h"
#include "L3_state_over.h"

// ACK 수신 상태 추적
static int currentSendIndex = 0;
static bool waitingAck = false;

void L3_handleMode2() {
    static bool allSent = false;

    if (!allSent) {
        printf("[HOST] Sending MODE:OVER to all players...\n");

        for (int i = 0; i < NUM_PLAYERS; i++) {
            if (players[i].id != myId) {
                L3_sendData(players[i].id, "MODE:OVER");
            }
        }

        allSent = true;
    }

    if (L3_event_checkEventFlag(L3_event_msgRcvd)) {
        uint8_t* msg = L2_LLI_getRcvdDataPtr();
        if (L2_msg_checkIfAck(msg)) {
            printf("[HOST] ACK received from player.\n");
        }

        L3_event_clearEventFlag(L3_event_msgRcvd);
    }

    // 일단 단순히 수신 여부 관계없이 OVER 상태로 넘어감
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

    // 이후 reset하거나 종료 대기할 수도 있음
}
