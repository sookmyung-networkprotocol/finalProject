#include "L3_shared.h"
#include "L3_LLinterface.h"
#include "L3_FSMevent.h"
#include "mbed.h"
#include <cstring>

void L3_handleDay()
{
    static bool announced = false;
    static size_t inputLen = 0;
    static char inputBuffer[100] = {0};

    // 첫 진입 안내 메시지 (한 번만 출력)
    if (!announced) {
        pc.printf("[DAY] 지금부터 낮입니다. 그룹 채팅을 시작합니다.\n");
        announced = true;
    }

    // 입력 인터럽트에서 저장된 originalWord 전송 처리
    if (L3_event_checkEventFlag(L3_event_dataToSend)) {
        pc.printf("[DEBUG] 메시지 전송 시작. wordLen=%d\n", wordLen);

        // 자신이 보낸 메시지 표시
        pc.printf("[YOU] %s\n", originalWord);

        for (int i = 0; i < NUM_PLAYERS; i++) {
            if (players[i].isAlive && players[i].id != myId && players[i].id != 1) {
                // 메시지 앞에 송신자 ID 추가 (예: "2|hello")
                char taggedMsg[120];
                snprintf(taggedMsg, sizeof(taggedMsg), "%d|%s", myId, originalWord);

                L3_LLI_dataReqFunc((uint8_t*)taggedMsg, strlen(taggedMsg), players[i].id);
                pc.printf("[DEBUG] → 메시지 전송 대상: ID %d\n", players[i].id);
            }
        }

        L3_event_clearEventFlag(L3_event_dataToSend);
    }

    // 그룹 채팅 수신 메시지 출력 처리
    if (L3_event_checkEventFlag(L3_event_msgRcvd)) {
        uint8_t* msg = L3_LLI_getMsgPtr();
        uint8_t size = L3_LLI_getSize();

        // 송신자 ID 추출 (ID|메시지 형식)
        int senderId = -1;
        char* sep = (char*)memchr(msg, '|', size);
        if (sep != NULL) {
            *sep = '\0';
            senderId = atoi((char*)msg);
            char* realMsg = sep + 1;
            pc.printf("[CHAT] ID %d: %s\n", senderId, realMsg);
        } else {
            pc.printf("[CHAT] 알 수 없는 형식: %.*s\n", size, msg);
        }

        L3_event_clearEventFlag(L3_event_msgRcvd);
    }

    // HOST가 'v' 입력하면 투표로 전이
    if (myId == 1 && pc.readable()) {
        char c = pc.getc();
        if (c == 'v') {
            pc.printf("[HOST] 투표를 시작합니다.\n");

            // 모든 게스트에게 vote 시작 신호 전송
            uint8_t signal[] = {'S'};
            for (int i = 0; i < NUM_PLAYERS; i++) {
                if (players[i].id != myId && players[i].isAlive) {
                    L3_LLI_dataReqFunc(signal, 1, players[i].id);
                }
            }

            main_state = VOTE;
        }
    }
}
