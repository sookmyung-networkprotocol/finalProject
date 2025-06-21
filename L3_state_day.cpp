// L3_state_day.cpp
#include "L3_shared.h"
#include "L3_FSMevent.h"
#include "L3_LLinterface.h"
#include <cstring>

void L3_handleDay()
{
    static char inputBuffer[256] = {0};
    static size_t inputLen = 0;
    static bool printedNotice = false;

    static int count = 0;
    if (count++ < 1)
        pc.printf("[DEBUG] L3_handleDay() 진입\n");


    // 안내 메시지는 한 번만 출력
    if (!printedNotice) {
        pc.printf("[DEBUG] 안내 메시지 출력 시작 (myId=%d)\n", myId);
        if (myId == 1) {
            pc.printf("[DAY] 지금은 낮입니다. 채팅에는 참여하지 않으며, 투표를 시작하려면 'v'를 입력하세요.\n");
        } else {
            pc.printf("[DAY] 지금부터 낮입니다. 그룹 채팅을 시작합니다.\n");
        }
        printedNotice = true;
    }

    // 죽은 사람은 행동 불가
    if (idead) {
        pc.printf("[DEBUG] 플레이어가 죽었기 때문에 바로 VOTE로 전이\n");
        pc.printf("[DAY] 당신은 죽었기 때문에 채팅할 수 없습니다.\n");
        main_state = VOTE;
        printedNotice = false;
        return;
    }

    // Host의 경우: 'v' 입력 시 VOTE로 전이
    if (myId == 1) {
        if (pc.readable()) {
            char ch = pc.getc();
            pc.printf("[DEBUG] HOST 키 입력 감지: '%c'\n", ch);
            if (ch == 'v' || ch == 'V') {
                pc.printf("[HOST] 투표를 시작합니다.\n");
                main_state = VOTE;
                printedNotice = false;
                return;
            }
        }
        return; // Host는 채팅 안 하므로 여기서 종료
    }

    // Guest: 키보드 입력 처리 (echo 포함)
    if (pc.readable()) {
        char ch = pc.getc();
        pc.printf("[DEBUG] GUEST 키 입력 감지: '%c'\n", ch);

        if (ch == '\r' || ch == '\n') {
            inputBuffer[inputLen] = '\0';
            strcpy((char*)originalWord, inputBuffer);
            wordLen = inputLen;
            inputLen = 0;
            L3_event_setEventFlag(L3_event_dataToSend);
            pc.printf("[DEBUG] 메시지 완료됨: %s\n", originalWord);
        } else {
            if (inputLen < sizeof(inputBuffer) - 1) {
                inputBuffer[inputLen++] = ch;
                pc.putc(ch);  // echo
            }
        }
    }

    // 전송 이벤트 감지
    if (L3_event_checkEventFlag(L3_event_dataToSend)) {
        pc.printf("[DEBUG] 메시지 전송 시작. wordLen=%d\n", wordLen);
        for (int i = 0; i < NUM_PLAYERS; i++) {
            if (players[i].isAlive && players[i].id != myId && players[i].id != 1) {
                L3_LLI_dataReqFunc(originalWord, wordLen, players[i].id);
                pc.printf("[DEBUG] → 메시지 전송 대상: ID %d\n", players[i].id);
            }
        }
        pc.printf("\n[YOU] %s\n", originalWord);
        wordLen = 0;
        L3_event_clearEventFlag(L3_event_dataToSend);
    }

    // 수신 이벤트 감지
    if (L3_event_checkEventFlag(L3_event_msgRcvd)) {
        uint8_t* msg = L3_LLI_getMsgPtr();
        uint8_t from = L3_LLI_getSrcId();
        uint8_t len = L3_LLI_getSize();

        pc.printf("[DEBUG] 메시지 수신 감지 ← from ID %d, length %d\n", from, len);
        pc.printf("[DAY] ID %d: %.*s\n", from, len, msg);

        L3_event_clearEventFlag(L3_event_msgRcvd);
    }
}
