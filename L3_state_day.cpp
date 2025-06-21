// L3_state_day.cpp
#include "L3_shared.h"
#include "L3_LLinterface.h"
#include "L3_FSMevent.h"
#include "L2_msg.h"
#include <cstring>

// 안전하게 L2 포맷 붙여 전송하는 함수
void L3_sendRaw(uint8_t* payload, uint8_t length, uint8_t destId)
{
    uint8_t pdu[200];
    uint8_t seq = 0;
    uint8_t size = L2_msg_encodeData(pdu, payload, seq, length, 1);
    pc.printf("[DEBUG] SEND pdu[0]=%d, size=%d, destId=%d\n", pdu[0], size, destId);
    L3_LLI_dataReqFunc(pdu, size, destId);
}

void L3_handleDay()
{
    static char inputBuffer[256] = {0};
    static int inputLen = 0;

    if (idead) {
        pc.printf("[DAY] 당신은 죽었기 때문에 채팅할 수 없습니다.\n");
        main_state = VOTE;
        return;
    }

    // 키보드 입력 수신
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

    // 메시지 송신 처리
    if (L3_event_checkEventFlag(L3_event_dataToSend)) {
        for (int i = 0; i < NUM_PLAYERS; i++) {
            if (players[i].isAlive && players[i].id != myId) {
                char taggedMsg[1030];
                snprintf(taggedMsg, sizeof(taggedMsg), "%d|%s", myId, originalWord);
                L3_sendRaw((uint8_t*)taggedMsg, strlen(taggedMsg), players[i].id);
            }
        }
        pc.printf("\n[YOU] %s\n", originalWord);
        wordLen = 0;
        L3_event_clearEventFlag(L3_event_dataToSend);
    }

    // 메시지 수신 처리
    if (L3_event_checkEventFlag(L3_event_msgRcvd)) {
        uint8_t* msg = L3_LLI_getMsgPtr();
        uint8_t from = L3_LLI_getSrcId();
        uint8_t len = L3_LLI_getSize();

        pc.printf("[DEBUG] RECEIVED from %d, size=%d, msg=%.*s\n", from, len, len, msg);

        char* sep = (char*)memchr(msg, '|', len);
        if (sep != NULL) {
            *sep = '\0';
            int senderId = atoi((char*)msg);
            char* content = sep + 1;
            pc.printf("[DAY] %d번 플레이어: %s\n", senderId, content);
        } else {
            pc.printf("[DAY] %d번 플레이어: %.*s\n", from, len, msg);
        }

        L3_event_clearEventFlag(L3_event_msgRcvd);
    }

    if (pc.readable()) {
        char ch = pc.getc();
        if (ch == 'v') {
            pc.printf("[DAY] 채팅 종료. 투표로 전환합니다.\n");
            main_state = VOTE;
        }
    }
}
