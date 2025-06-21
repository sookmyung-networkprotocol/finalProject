// L3_state_vote.cpp
#include "L3_shared.h"
#include "L3_FSMevent.h"
#include "L3_LLinterface.h"
#include <cstring>
#include "L3_host.h"

void L3_handleVote()
{
    static bool initialized = false;
    static int voteCount[NUM_PLAYERS] = {0};
    static int receivedVotes = 0;

    if (!initialized) {
        memset(voteCount, 0, sizeof(voteCount));
        receivedVotes = 0;
        pc.printf("[VOTE] 투표를 시작합니다.\n");

        if (myId == 1) {
            // HOST: 생존 플레이어에게 투표 요청
            for (int i = 0; i < NUM_PLAYERS; i++) {
                if (players[i].isAlive && players[i].id != 1) {
                    char msg[] = "VOTE_REQ";
                    L3_LLI_dataReqFunc((uint8_t*)msg, strlen(msg), players[i].id);
                }
            }
        } else {
            // GUEST: 투표 대상 ID 직접 입력 및 전송
            pc.printf("[VOTE] 생존자 ID를 입력하세요: ");
            char input = pc.getc();
            pc.putc(input);

            if (input >= '0' && input <= '9') {
                uint8_t voteMsg[2] = {'V', (uint8_t)(input - '0')};
                L3_LLI_dataReqFunc(voteMsg, 2, 1); // HOST에게 전송
                pc.printf(" → %c번에게 투표 전송 완료\n", input);
            }
        }
        initialized = true;
    }

    if (L3_event_checkEventFlag(L3_event_msgRcvd)) {
        uint8_t* dataPtr = L3_LLI_getMsgPtr();
        uint8_t fromId = L3_LLI_getSrcId();

        if (myId == 1 && dataPtr[0] == 'V') {
            int votedId = dataPtr[1];
            voteCount[votedId]++;
            receivedVotes++;
            pc.printf("[HOST] %d번이 %d번에게 투표함\n", fromId, votedId);
        }

        L3_event_clearEventFlag(L3_event_msgRcvd);
    }

    if (myId == 1 && receivedVotes >= NUM_PLAYERS - 1) {
        int maxVotes = 0, eliminatedId = -1;
        for (int i = 0; i < NUM_PLAYERS; i++) {
            if (voteCount[i] > maxVotes && players[i].isAlive) {
                maxVotes = voteCount[i];
                eliminatedId = i;
            }
        }

        // 결과 전송
        for (int i = 0; i < NUM_PLAYERS; i++) {
            if (!players[i].isAlive) continue;
            char result[64];
            if (eliminatedId != -1) {
                sprintf(result, "[VOTE RESULT] %d번 처형됨", eliminatedId);
                if (players[i].id == eliminatedId) {
                    players[i].isAlive = false;
                    if (myId == eliminatedId) idead = true;
                }
            } else {
                sprintf(result, "[VOTE RESULT] 아무도 처형되지 않음");
            }
            L3_LLI_dataReqFunc((uint8_t*)result, strlen(result), players[i].id);
        }

        if (checkGameOver()) {
            main_state = OVER;
        } else {
            main_state = NIGHT;
        }

        initialized = false;
    }
}
