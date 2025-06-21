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

void L3service_processInputWord(void)
{
    static bool printedInputLog = false;
    static size_t inputLen = 0;
    char c = pc.getc();

    if (!printedInputLog) {
        pc.printf("[DEBUG] L3service_processInputWord 작동 시작 (첫 문자='%c')\n", c);
        printedInputLog = true;
    }

    if (!L3_event_checkEventFlag(L3_event_dataToSend)) {
        if (c == '\n' || c == '\r') {
            if (inputLen > 0) {
                originalWord[inputLen] = '\0';           // 문자열 종료
                wordLen = inputLen;                      // 💥 핵심 수정
                memcpy(sdu, originalWord, wordLen);      // 복사
                L3_event_setEventFlag(L3_event_dataToSend);
                pc.printf("[DEBUG] 입력 완료. wordLen=%d, word='%s'\n", wordLen, originalWord);  // 로그
            }
            inputLen = 0;  // 다음 입력을 위해 초기화
        } else {
            if (inputLen < sizeof(originalWord) - 1)
                originalWord[inputLen++] = c;
        }
    }

}


// NIGHT 단계 종료 시 결과 반영
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

// FSM 실행 루프
void L3_FSMrun() {
    // 상태 전이 감지 및 attach 처리
    if (main_state != prev_state) {
        pc.printf("\n[L3] 상태 전이: %d → %d\n", prev_state, main_state);

        // 입력 인터럽트 설정
        if (main_state == DAY) {
            if (myId != 1)
                pc.attach(&L3service_processInputWord, Serial::RxIrq);
            else
                pc.attach(NULL, Serial::RxIrq);
        } else {
            pc.attach(NULL, Serial::RxIrq);
        }

        // 밤에서 낮으로 전이되는 경우에만 실행
        if (main_state == DAY && prev_state == POLICE) {
            L3_finalizeNight();
        }

        prev_state = main_state;
    }

    // Guest가 vote start 신호 'S'를 수신한 경우 상태 전이
    if (myId != 1 && main_state == DAY && L3_event_checkEventFlag(L3_event_msgRcvd)) {
        uint8_t* msg = L3_LLI_getMsgPtr();
        uint8_t size = L3_LLI_getSize();
        if (size == 1 && msg[0] == 'S') {
            pc.printf("[DEBUG] 투표 시작 신호 수신 → VOTE 상태로 전이\n");
            main_state = VOTE;
        }
        L3_event_clearEventFlag(L3_event_msgRcvd);
    }

    switch (main_state) {
        case L3STATE_IDLE:     L3_handleIdle(); break;
        case MATCH:            L3_handleMatch(); break;
        case MODE_1:           L3_handleMode1(); break;

        case DAY: {
            static int count = 0;
            if (count++ < 1)
                pc.printf("[DEBUG] case DAY 진입\n");

            if (prev_state == POLICE) {
                L3_finalizeNight();
            }

            // Host가 v 입력 시 투표 시작
            if (myId == 1 && pc.readable()) {
                char c = pc.getc();
                pc.printf("[DEBUG] HOST 키 입력 감지: '%c'\n", c);
                if (c == 'v') {
                    pc.printf("[HOST] 투표를 시작합니다.\n");
                    // 모든 게스트에게 vote 시작 신호 전송
                    uint8_t signal[] = {'S'};
                    for (int i = 0; i < NUM_PLAYERS; i++) {
                        if (players[i].isAlive && players[i].id != myId) {
                            L3_LLI_dataReqFunc(signal, 1, players[i].id);
                        }
                    }
                    main_state = VOTE;
                    return;
                }
            }

            L3_handleDay();
            break;
        }

        case VOTE:             L3_handleVote(); break;
        case NIGHT:            L3_handleMafia(); break;
        case DOCTOR:           L3_handleDoctor(); break;
        case POLICE:           L3_handlePolice(); break;
        case MODE_2:           L3_handleMode2(); break;
        case OVER:             L3_handleOver(); break;
        case TYPING:           break;
    }
}

