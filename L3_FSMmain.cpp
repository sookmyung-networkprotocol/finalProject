// L3_FSMmain.cpp
#include "L3_shared.h"
#include "L3_host.h"
#include "L3_FSMevent.h"
#include "L3_LLinterface.h"
#include "L3_state_idle.h"
#include "L3_state_day.h"
#include "L3_state_vote.h"
#include "L3_state_night.h"
#include "L3_state_over.h"
#include <cstring>
#include <cstdio>

void L3service_processInputWord();

void L3_initFSM(uint8_t id, uint8_t destId) {
    myId = id;
    myDestId = destId;
    pc.attach(&L3service_processInputWord, Serial::RxIrq);
    printf("[L3] FSM initialized. MyID: %d, DestID: %d\n", myId, myDestId);
}

void L3service_processInputWord() {
    char c = pc.getc();
    if (!L3_event_checkEventFlag(L3_event_dataToSend)) {
        if (c == '\n' || c == '\r') {
            originalWord[wordLen++] = '\0';
            memcpy(sdu, originalWord, wordLen);
            L3_event_setEventFlag(L3_event_dataToSend);
            wordLen = 0;
        } else {
            originalWord[wordLen++] = c;
        }
    }
}

void L3_finalizeNight() {
    if (killedId != -1) {
        if (killedId == doctorTarget) {
            pc.printf("[NIGHT] 의사가 %d번을 살려냈습니다.\n", killedId);
        } else {
            pc.printf("[NIGHT] %d번 플레이어가 죽었습니다.\n", killedId);
            for (int i = 0; i < NUM_PLAYERS; i++) {
                if (players[i].id == killedId) {
                    players[i].isAlive = false;
                    if (myId == killedId) idead = true;
                }
            }
        }
    }
    killedId = -1;
    doctorTarget = -1;
}

void L3_FSMrun() {
    if (main_state != prev_state) {
        pc.printf("\n[L3] 상태 전이: %d → %d\n", prev_state, main_state);
        prev_state = main_state;
    }

    switch (main_state) {
        case L3STATE_IDLE:
            L3_handleIdle();
            break;

        case MATCH:
            L3_handleMatch();
            break;

        case MODE_1:
            L3_handleMode1();
            break;

        case DAY:
            L3_handleDay();
            break;

        case VOTE:
            L3_handleVote();
            break;

        case NIGHT:
            L3_handleMafia();
            break;

        case DOCTOR:
            L3_handleDoctor();
            break;

        case POLICE:
            L3_handlePolice();
            break;

        case MODE_2:
            L3_handleMode2();
            break;

        case OVER:
            L3_handleOver();
            break;

        case TYPING:
            break;
    }

    if (main_state == DAY && prev_state == POLICE) {
        L3_finalizeNight();
    }
}
