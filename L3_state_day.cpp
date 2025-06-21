// L3_state_day.cpp
#include "L3_shared.h"
#include "L3_FSMevent.h"
#include "L3_LLinterface.h"
#include <cstring>

void L3_handleDay()
{
    static char inputBuffer[256] = {0};
    static int inputLen = 0;

    if (idead) {
        pc.printf("[DAY] 당신은 죽었기 때문에 채팅할 수 없습니다.\n");
        main_state = VOTE;
        return;
    }

    // [HOST] - 채팅은 안 하고, 'v' 입력만 처리
    if (myId == 1) {
        if (pc.readable()) {
            char ch = pc.getc();
            if (ch == 'v' || ch == 'V') {
                pc.printf("\n[HOST] VOTE 단계로 넘어갑니다.\n");
                main_state = VOTE;
            }
        }
        return;  // HOST는 여기서 끝
    }

    // [PLAYER] - 그룹 채팅 처리
    if (pc.readable()) {
        char ch = pc.getc();

        if (ch == '\r' || ch == '\n') {
            inputBuffer[inputLen] = '\0';
            strcpy((char*)originalWord, inputBuffer);
            wordLen = inputLen;
            inputLen = 0;
            L3_event_setEventFlag(L3_event_dataToSend);
        } else {
            if (inputLen < sizeof(inputBuffer) - 1) {
                inputBuffer[inputLen++] = ch;
                pc.putc(ch);  // echo
            }
        }
    }

    // 메시지 전송
    if (L3_event_checkEventFlag(L3_event_dataToSend)) {
        for (int i = 0; i < NUM_PLAYERS; i++) {
            if (players[i].isAlive && players[i].id != myId && players[i].id != 1) {
                L3_LLI_dataReqFunc(originalWord, wordLen, players[i].id);
            }
        }
        pc.printf("\n[YOU] %s\n", originalWord);
        wordLen = 0;
        L3_event_clearEventFlag(L3_event_dataToSend);
    }

    // 메시지 수신
    if (L3_event_checkEventFlag(L3_event_msgRcvd)) {
        uint8_t* msg = L3_LLI_getMsgPtr();
        uint8_t from = L3_LLI_getSrcId();
        uint8_t len = L3_LLI_getSize();

        pc.printf("[DAY] %d번 플레이어: %.*s\n", from, len, msg);

        L3_event_clearEventFlag(L3_event_msgRcvd);
    }
}
