#include "L3_shared.h"
#include "L3_LLinterface.h"
#include "L3_FSMevent.h"
#include "mbed.h"
#include <cstring>

#define MAX_PLAYERS 4
static uint8_t knownPlayerIDs[MAX_PLAYERS];
static int knownPlayerCount = 0;

void L3_handleDay()
{
    static bool printedOnce = false;
    static bool initialized = false;
    
    if (!printedOnce) {
        pc.printf("\n[DAY] 낮이 되었습니다. 단체 채팅을 시작합니다.\n");
        pc.printf("메시지를 입력하고 Enter를 누르세요. ('v' 입력 시 투표로 이동)\n");
        printedOnce = true;
    }
    
    if (idead) {
        pc.printf("\n☠️ 당신은 사망했기 때문에 채팅에 참여할 수 없습니다.\n");
        printedOnce = false;
        initialized = false;  // 추가
        main_state = VOTE;
        return;
    }
    
    if (!initialized) {
        knownPlayerCount = 0;
        for (int i = 0; i < NUM_PLAYERS; i++) {
            if (players[i].isAlive && players[i].id != myId) {
                knownPlayerIDs[knownPlayerCount++] = players[i].id;
            }
        }
        initialized = true;
    }
    
    // 키보드 입력 처리를 하나로 통합
    if (pc.readable()) {
        char c = pc.getc();
        
        // 'v' 입력 체크를 먼저
        if (c == 'v' && wordLen == 0) {
            pc.printf("\n[DAY] 채팅을 종료하고 투표로 전환합니다.\n");
            printedOnce = false;
            initialized = false;
            main_state = VOTE;
            return;
        }
        
        // 일반 메시지 입력 처리
        if (!L3_event_checkEventFlag(L3_event_dataToSend)) {
            if (c == '\n' || c == '\r') {
                originalWord[wordLen] = '\0';  // wordLen 증가 없이
                L3_event_setEventFlag(L3_event_dataToSend);
            } else if (wordLen < L3_MAXDATASIZE - 1) {  // 버퍼 오버플로우 방지
                originalWord[wordLen++] = c;
            }
        }
    }
    
    // 메시지 전송
    if (L3_event_checkEventFlag(L3_event_dataToSend)) {
        pc.printf("\n[YOU] %s\n", originalWord);
        for (int i = 0; i < knownPlayerCount; i++) {
            L3_LLI_dataReqFunc(originalWord, wordLen, knownPlayerIDs[i]);
        }
        wordLen = 0;
        L3_event_clearEventFlag(L3_event_dataToSend);
    }
    
    // 수신 메시지 처리
    if (L3_event_checkEventFlag(L3_event_msgRcvd)) {
        uint8_t* msg = L3_LLI_getMsgPtr();
        int fromId = L3_LLI_getSrcId();
        pc.printf("\n[Player %d] %s\n", fromId, msg);
        L3_event_clearEventFlag(L3_event_msgRcvd);
    }
}