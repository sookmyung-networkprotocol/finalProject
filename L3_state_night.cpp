// L3_state_night.cpp
#include "L3_state_night.h"
#include "L3_shared.h"
#include "L3_LLinterface.h"
#include "L3_FSMevent.h"
#include <cstring>

void sendAliveListToRoleWithExclusion(Role role) {
    char buffer[64] = "Alive: ";
    for (int i = 0; i < NUM_PLAYERS; i++) {
        if (players[i].isAlive && players[i].role != role && players[i].id != myId) {
            char temp[8];
            sprintf(temp, "%d ", players[i].id);
            strcat(buffer, temp);
        }
    }
    pc.printf("[%s] 선택지 전송 → %s\n", getRoleName(role), buffer);
}

bool isRoleDead(Role role) {
    for (int i = 0; i < NUM_PLAYERS; i++) {
        if (players[i].role == role && players[i].isAlive) return false;
    }
    return true;
}

void L3_handleMafia() {
    if (idead || strcmp(myRoleName, "Mafia") != 0) return;

    sendAliveListToRoleWithExclusion(ROLE_MAFIA);
    pc.printf("[MAFIA] 누구를 죽일까요? ID 입력: ");

    char input = pc.getc();
    if (input >= '0' && input <= '9') {
        int targetId = input - '0';
        pc.printf(" → 선택: ID %d\n", targetId);
        killedId = targetId; // shared에 반영
        strcpy((char*)sdu, "KILL");
        wordLen = 4;
        myDestId = targetId;
        L3_LLI_dataReqFunc(sdu, wordLen, myDestId);
        main_state = DOCTOR;
    }
}

void L3_handleDoctor() {
    if (idead || strcmp(myRoleName, "Doctor") != 0) {
        main_state = POLICE;
        return;
    }

    sendAliveListToRoleWithExclusion(ROLE_DOCTOR);
    pc.printf("[DOCTOR] 누구를 살릴까요? ID 입력: ");

    char input = pc.getc();
    if (input >= '0' && input <= '9') {
        int targetId = input - '0';
        pc.printf(" → 선택: ID %d\n", targetId);
        doctorTarget = targetId;
        main_state = POLICE;
    }
}

void L3_handlePolice() {
    if (idead || strcmp(myRoleName, "Police") != 0) {
        main_state = DAY;
        return;
    }

    sendAliveListToRoleWithExclusion(ROLE_POLICE);
    pc.printf("[POLICE] 누구를 조사할까요? ID 입력: ");

    char input = pc.getc();
    if (input >= '0' && input <= '9') {
        int targetId = input - '0';
        for (int i = 0; i < NUM_PLAYERS; i++) {
            if (players[i].id == targetId) {
                pc.printf(" → 조사 결과: %s\n",
                          players[i].role == ROLE_MAFIA ? "Mafia" : "Not Mafia");
                break;
            }
        }
        main_state = DAY;
    }
}
