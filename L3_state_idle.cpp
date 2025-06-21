// L3_state_idle.cpp
#include "L3_state_idle.h"
#include "L3_shared.h"
#include "L3_LLinterface.h"
#include "L3_FSMevent.h"
#include <cstring>

// IDLE 상태: HOST가 'start'를 입력하면 MATCH로 이동
void L3_handleIdle() {
    static char buffer[64] = {0};
    static int len = 0;

    if (idead) return;

    if (pc.readable()) {
        char ch = pc.getc();
        if (ch == '\r' || ch == '\n') {
            buffer[len] = '\0';
            if (strcmp(buffer, "start") == 0 && myId == 1) {
                pc.printf("[IDLE] start 입력 확인 → 매칭 상태 진입\n");
                change_state = 0;
                main_state = MATCH;
            }
            len = 0;
        } else {
            if (len < sizeof(buffer) - 1) {
                buffer[len++] = ch;
                pc.putc(ch);
            }
        }
    }

    if (L3_event_checkEventFlag(L3_event_dataToSend)) {
        strcpy((char*)sdu, (char*)originalWord);
        L3_LLI_dataReqFunc(sdu, wordLen, myDestId);
        wordLen = 0;
        L3_event_clearEventFlag(L3_event_dataToSend);
    }

    if (L3_event_checkEventFlag(L3_event_msgRcvd)) {
        uint8_t* dataPtr = L3_LLI_getMsgPtr();
        uint8_t size = L3_LLI_getSize();
        pc.printf("[IDLE] 수신된 메시지: %.*s (len: %d)\n", size, dataPtr, size);
        L3_event_clearEventFlag(L3_event_msgRcvd);
    }
}

// MATCH 상태: HOST가 요청 전송, 클라이언트는 ACK 회신
void L3_handleMatch() {
    static int playerCnt = 1; // HOST 포함
    static bool reqSent = false;

    if (myId == 1 && !reqSent) {
        const char* reqMsg = "MATCH_REQ";
        for (int i = 2; i <= 8; i++) {
            L3_LLI_dataReqFunc((uint8_t*)reqMsg, strlen(reqMsg), i);
        }
        pc.printf("[MATCH] MATCH_REQ 전송 완료\n");
        reqSent = true;
    }

    if (myId == 1 && L3_event_checkEventFlag(L3_event_msgRcvd)) {
        uint8_t* msg = L3_LLI_getMsgPtr();
        uint8_t src = L3_LLI_getSrcId();

        if (strncmp((char*)msg, "MATCH_ACK", 9) == 0) {
            pc.printf("[MATCH] MATCH_ACK 수신 from %d\n", src);
            playerCnt++;
        }

        if (playerCnt >= NUM_PLAYERS) {
            pc.printf("[MATCH] 모든 플레이어 매칭 완료 → MODE_1 전이\n");
            main_state = MODE_1;
        }
        L3_event_clearEventFlag(L3_event_msgRcvd);
    }

    if (myId != 1 && L3_event_checkEventFlag(L3_event_msgRcvd)) {
        uint8_t* msg = L3_LLI_getMsgPtr();
        if (strncmp((char*)msg, "MATCH_REQ", 9) == 0) {
            pc.printf("[MATCH] MATCH_REQ 수신됨 → MATCH_ACK 전송\n");
            const char* ack = "MATCH_ACK";
            L3_LLI_dataReqFunc((uint8_t*)ack, strlen(ack), 1);
            main_state = MODE_1;
        }
        L3_event_clearEventFlag(L3_event_msgRcvd);
    }
}

// MODE_1 상태: 역할 전송 및 ACK 수신 → 게임 시작 준비
void L3_handleMode1() {
    static bool waitingAck = false;
    static bool waitingInput = false;
    static int currentIndex = 0;

    if (myId == 1 && change_state == 0) {
        createPlayers();
        for (int i = 0; i < NUM_PLAYERS; i++) {
            pc.printf("[MODE_1] Player %d - ID: %d, Role: %s\n", i, players[i].id, getRoleName(players[i].role));
        }
        waitingAck = false;
        waitingInput = false;
        currentIndex = 0;
        change_state = 1;
    }

    if (myId == 1 && change_state == 1) {
        if (!waitingAck && !waitingInput && currentIndex < NUM_PLAYERS) {
            myDestId = players[currentIndex].id;
            strcpy(msgStr, getRoleName(players[currentIndex].role));
            pc.printf("[MODE_1] 역할 전송 → ID %d : %s\n", myDestId, msgStr);
            L3_LLI_dataReqFunc((uint8_t*)msgStr, strlen(msgStr), myDestId);
            waitingAck = true;
        }

        if (L3_event_checkEventFlag(L3_event_msgRcvd)) {
            uint8_t* dataPtr = L3_LLI_getMsgPtr();
            if (strncmp((char*)dataPtr, "ACK", 3) == 0 && waitingAck) {
                pc.printf("[MODE_1] ACK 수신 from ID %d\n", myDestId);
                waitingAck = false;
                waitingInput = true;
                pc.printf("[MODE_1] 다음 역할 전송하려면 1 입력: ");
            }
            L3_event_clearEventFlag(L3_event_msgRcvd);
        }

        if (waitingInput && pc.readable()) {
            char c = pc.getc();
            if (c == '1') {
                pc.printf(" → 확인됨. 다음 전송 진행.\n");
                waitingInput = false;
                currentIndex++;
                if (currentIndex >= NUM_PLAYERS) {
                    pc.printf("[MODE_1] 모든 역할 전송 완료 → DAY 상태 진입\n");
                    change_state = 2;
                } else {
                    change_state = 1;
                    main_state = L3STATE_IDLE;
                }
            }
        }
    }

    if (myId != 1 && L3_event_checkEventFlag(L3_event_msgRcvd)) {
        uint8_t* dataPtr = L3_LLI_getMsgPtr();
        uint8_t size = L3_LLI_getSize();
        int len = (size > 15) ? 15 : size;
        memcpy(myRoleName, dataPtr, len);
        myRoleName[len] = '\0';

        pc.printf("[MODE_1] 역할 수신 완료: %s\n", myRoleName);
        const char ackMsg[] = "ACK";
        L3_LLI_dataReqFunc((uint8_t*)ackMsg, strlen(ackMsg), 1);
        L3_event_clearEventFlag(L3_event_msgRcvd);
        change_state = 2;
    }

    if (change_state == 2)
        main_state = DAY;
}
