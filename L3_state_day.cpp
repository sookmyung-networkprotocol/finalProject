#include "L3_shared.h"
#include "L3_LLinterface.h"
#include "L3_FSMevent.h"
#include "mbed.h"
#include <cstring>

// 최대 플레이어 수 정의
#define MAX_PLAYERS 4
static uint8_t knownPlayerIDs[MAX_PLAYERS];
static int knownPlayerCount = 0;

void L3_handleDay()
{
    static bool printedOnce = false;
    static bool initialized = false;

    // 안내 메시지 (한 번만 출력)
    if (!printedOnce) {
        pc.printf("\n🌞 [DAY] 낮이 되었습니다. 단체 채팅을 시작합니다.\n");
        pc.printf("✏️ 메시지를 입력하고 Enter를 누르세요. ('v' 입력 시 투표로 이동)\n");
        printedOnce = true;
    }

    // 사망자 처리
    if (idead) {
        pc.printf("\n☠️ 당신은 사망했기 때문에 채팅에 참여할 수 없습니다.\n");
        printedOnce = false;
        main_state = VOTE;
        return;
    }

    // 수신할 대상 목록 초기화 (한 번만)
    if (!initialized) {
        knownPlayerCount = 0;
        for (int i = 0; i < NUM_PLAYERS; i++) {
            if (players[i].isAlive && players[i].id != myId) {
                knownPlayerIDs[knownPlayerCount++] = players[i].id;
            }
        }
        initialized = true;
    }

    // 키보드 입력 수신 → originalWord에 저장
    if (pc.readable()) {
        char c = pc.getc();
        if (!L3_event_checkEventFlag(L3_event_dataToSend)) {
            if (c == '\n' || c == '\r') {
                originalWord[wordLen++] = '\0';
                L3_event_setEventFlag(L3_event_dataToSend);
            } else {
                originalWord[wordLen++] = c;
                if (wordLen >= L3_MAXDATASIZE - 1) {
                    originalWord[wordLen++] = '\0';
                    L3_event_setEventFlag(L3_event_dataToSend);
                }
            }
        }
    }

    // 전송 처리
    if (L3_event_checkEventFlag(L3_event_dataToSend)) {
        // 내 메시지 출력
        pc.printf("\n🗨️ [나] %s\n", originalWord);

        // 다른 플레이어에게 직접 전송
        for (int i = 0; i < knownPlayerCount; i++) {
            L3_LLI_dataReqFunc(originalWord, wordLen, knownPlayerIDs[i]);
        }

        wordLen = 0;
        L3_event_clearEventFlag(L3_event_dataToSend);
    }

    // 수신 처리
    if (L3_event_checkEventFlag(L3_event_msgRcvd)) {
        uint8_t* msg = L3_LLI_getMsgPtr();
        int fromId = L3_LLI_getSrcId();
        pc.printf("\n📨 [Player %d] %s\n", fromId, msg);
        L3_event_clearEventFlag(L3_event_msgRcvd);
    }

    // 'v' 입력 시 투표 상태로 전이
    if (pc.readable()) {
        char c = pc.getc();
        if (c == 'v') {
            pc.printf("\n🗳️ 채팅 종료. 투표를 시작합니다.\n");
            printedOnce = false;
            initialized = false;
            main_state = VOTE;
        }
    }
}
