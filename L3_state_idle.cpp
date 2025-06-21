// L3_state_idle.cpp
#include "L3_state_idle.h"
#include "L3_shared.h"
#include "L3_LLinterface.h"
#include "L3_FSMevent.h"
#include "L2_FSMmain.h"
#include "L3_host.h"
#include <cstring>
#include <cstdio>

void L3_handleIdle() {
    if (change_state == 0) {
        main_state = MATCH;
    } else if (change_state == 1) {
        L2_FSMrun();
        main_state = MODE_1;
    }

    if (L3_event_checkEventFlag(L3_event_msgRcvd)) {
        uint8_t* dataPtr = L3_LLI_getMsgPtr();
        uint8_t size = L3_LLI_getSize();
        printf("\r\nRECEIVED MSG : %s (length:%i)\n\n", dataPtr, size);
        L3_event_clearEventFlag(L3_event_msgRcvd);
    } else if (L3_event_checkEventFlag(L3_event_dataToSend)) {
        strcpy((char*)sdu, (char*)originalWord);
        L3_LLI_dataReqFunc(sdu, wordLen, myDestId);
        wordLen = 0;
        L3_event_clearEventFlag(L3_event_dataToSend);
    }
}

void L3_handleMatch() {
    if (myId == 1) {
        createPlayers();
        for (int i = 0; i < NUM_PLAYERS; i++) {
            pc.printf("\r\nPlayer %d - ID: %d, Role: %s\n\n", i, players[i].id, getRoleName(players[i].role));
        }
        change_state = 1;
        main_state = MODE_1;
    } else {
        // 게스트도 상태 전이 필요
        main_state = MODE_1;
    }
}


void L3_handleMode1() {
    static bool waitingAck = false;
    static bool waitingHostInput = false;
    static int currentSendIndex = 0;

    // Host 전용 처리
    if (myId == 1 && change_state == 1) {
        // 아직 역할을 다 보내지 않았고, 대기 중이 아닐 때
        if (!waitingAck && !waitingHostInput && currentSendIndex < NUM_PLAYERS) {
            myDestId = players[currentSendIndex].id;
            strcpy(msgStr, getRoleName(players[currentSendIndex].role));
            strcpy(players[currentSendIndex].receivedRole, msgStr);  // 역할 기억
            pc.printf("\r\nSEND ROLE to ID %d : %s\n\n", myDestId, msgStr);

            // 역할 전송
            L3_LLI_dataReqFunc((uint8_t*)msgStr, strlen(msgStr), myDestId);
            waitingAck = true;
        }

        // ACK 수신 처리
        if (L3_event_checkEventFlag(L3_event_msgRcvd)) {
            uint8_t* dataPtr = L3_LLI_getMsgPtr();
            uint8_t size = L3_LLI_getSize();

            if (size == 3 && strncmp((char*)dataPtr, "ACK", 3) == 0 && waitingAck) {
                pc.printf("\r\nACK received from player ID %d\n", myDestId);
                waitingAck = false;
                waitingHostInput = true;
                pc.printf("\r\n다음 플레이어에게 전송할까요? (1 입력)\n");
            }

            L3_event_clearEventFlag(L3_event_msgRcvd);
        }

        // Host가 '1' 입력 시 다음으로 넘어감
        if (waitingHostInput) {
            if (pc.readable()) {
                char c = pc.getc();
                pc.printf("\n[DEBUG] 입력된 문자 (ASCII): '%c' (0x%02X)\n", c, c);

                if (c == '1') {
                    pc.printf("\r\n1 입력 확인. 다음 플레이어로 전송합니다.\n");
                    waitingHostInput = false;
                    currentSendIndex++;

                    if (currentSendIndex >= NUM_PLAYERS) {
                        pc.printf("\r\n모든 플레이어에게 역할을 전송했습니다. 게임을 시작합니다.\n\n");
                        change_state = 2;
                        main_state = DAY;
                    } else {
                        change_state = 1; // 다음 플레이어 전송 준비
                        main_state = L3STATE_IDLE;
                    }
                } else if (c == '\r' || c == '\n') {
                    // 엔터 무시
                    pc.printf("[INFO] 엔터 입력 무시됨. '1'을 눌러주세요.\n");
                } else {
                    pc.printf("[INFO] 입력 무시됨: '%c' (0x%02X). '1'을 입력해야 다음으로 진행됩니다.\n", c, c);
                }
            }
        }

    // Guest 플레이어 처리
    if (myId != 1 && L3_event_checkEventFlag(L3_event_msgRcvd)) {
        uint8_t* dataPtr = L3_LLI_getMsgPtr();
        uint8_t size = L3_LLI_getSize();
        int len = (size > 15) ? 15 : size;

        memcpy(myRoleName, dataPtr, len);
        myRoleName[len] = '\0';

        pc.printf("\r\n나의 역할 : %.*s (length:%d)\n\n", size, myRoleName, size);

        // ACK 전송
        const char ackMsg[] = "ACK";
        L3_LLI_dataReqFunc((uint8_t*)ackMsg, sizeof(ackMsg) - 1, 1);

        L3_event_clearEventFlag(L3_event_msgRcvd);
        pc.printf("\r\n게임이 시작되었습니다.\n\n");
        change_state = 2;
        main_state = DAY;
    }

    // 상태 전이
    if (change_state == 2)
        main_state = DAY;
}
