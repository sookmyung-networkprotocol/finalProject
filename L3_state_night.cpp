#include "L3_shared.h"
#include "L3_state_night.h"

// 내부 저장 변수
static int mafiaTarget = -1;
static int doctorTarget = -1;

// -------------------------------------------------
// 도우미 함수: 특정 역할이 모두 죽었는지 확인
bool isRoleDead(Role role) {
    for (int i = 0; i < NUM_PLAYERS; i++) {
        if (players[i].role == role && players[i].isAlive)
            return false;
    }
    return true;
}

// 도우미 함수: 특정 역할에게 보여줄 대상 ID 목록을 역할별 정책에 따라 전송
void sendAliveListToRoleWithExclusion(Role role) {
    for (int i = 0; i < NUM_PLAYERS; i++) {
        if (players[i].role == role && players[i].isAlive) {
            uint8_t senderId = players[i].id;
            char msg[64] = {0};
            strcpy(msg, "ROLE|");

            for (int j = 0; j < NUM_PLAYERS; j++) {
                if (!players[j].isAlive) continue;

                bool exclude = false;

                if ((role == ROLE_MAFIA || role == ROLE_POLICE) && players[j].id == senderId)
                    exclude = true;

                if (!exclude) {
                    char idStr[4];
                    sprintf(idStr, "%d,", players[j].id);
                    strcat(msg, idStr);
                }
            }

            L3_sendData(senderId, msg);
            printf("[HOST] Sent to %s (ID %d): %s\n", getRoleName(role), senderId, msg);
        }
    }
}

// -------------------------------------------------
// 마피아 FSM 상태 처리
void L3_handleMafia() {
    if (isRoleDead(ROLE_MAFIA)) {
        mafiaTarget = -1;
        main_state = DOCTOR;
        return;
    }

    sendAliveListToRoleWithExclusion(ROLE_MAFIA);

    if (L3_event_checkEventFlag(L3_event_msgRcvd)) {
        uint8_t* data = L2_LLI_getRcvdDataPtr();
        mafiaTarget = atoi((char*)&data[0]);

        printf("[HOST] Mafia selected to kill: %d\n", mafiaTarget);
        L3_event_clearEventFlag(L3_event_msgRcvd);

        main_state = DOCTOR;
    }
}

// 의사 FSM 상태 처리
void L3_handleDoctor() {
    if (isRoleDead(ROLE_DOCTOR)) {
        doctorTarget = -1;
        main_state = POLICE;
        return;
    }

    sendAliveListToRoleWithExclusion(ROLE_DOCTOR);

    if (L3_event_checkEventFlag(L3_event_msgRcvd)) {
        uint8_t* data = L2_LLI_getRcvdDataPtr();
        doctorTarget = atoi((char*)&data[0]);

        printf("[HOST] Doctor selected to save: %d\n", doctorTarget);
        L3_event_clearEventFlag(L3_event_msgRcvd);

        main_state = POLICE;
    }
}

// 경찰 FSM 상태 처리
void L3_handlePolice() {
    if (!isRoleDead(ROLE_POLICE)) {
        sendAliveListToRoleWithExclusion(ROLE_POLICE);
    }

    if (L3_event_checkEventFlag(L3_event_msgRcvd)) {
        uint8_t* data = L2_LLI_getRcvdDataPtr();
        int suspectId = atoi((char*)&data[0]);

        const char* resultMsg = "NO";
        for (int i = 0; i < NUM_PLAYERS; i++) {
            if (players[i].id == suspectId && players[i].role == ROLE_MAFIA) {
                resultMsg = "YES";
                break;
            }
        }

        for (int i = 0; i < NUM_PLAYERS; i++) {
            if (players[i].role == ROLE_POLICE && players[i].isAlive) {
                L3_sendData(players[i].id, resultMsg);
                printf("[HOST] Sent investigation result to Police (ID %d): %s\n", players[i].id, resultMsg);
            }
        }

        L3_event_clearEventFlag(L3_event_msgRcvd);
    }

    // 경찰 처리 후, 밤 행동 요약 및 생사 판정
    if (mafiaTarget != -1 && mafiaTarget != doctorTarget) {
        for (int i = 0; i < NUM_PLAYERS; i++) {
            if (players[i].id == mafiaTarget) {
                players[i].isAlive = false;
                printf("[HOST] Player %d has been killed by Mafia.\n", mafiaTarget);
                break;
            }
        }
    } else {
        printf("[HOST] No one died tonight. Doctor saved the target or Mafia did not act.\n");
    }

    // 상태 초기화
    mafiaTarget = -1;
    doctorTarget = -1;

    main_state = DAY;
}
