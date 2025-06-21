// L3_state_vote.cpp
#include "L3_shared.h"
#include "L3_FSMevent.h"
#include "L3_LLinterface.h"
#include <cstring>
#include <cstdlib>

static int voteCounts[NUM_PLAYERS] = {0};
static bool printedOnce = false;
static int totalVotesReceived = 0;

void L3_handleVote()
{
    if (!printedOnce) {
        pc.printf("\n[VOTE] 투표를 시작합니다.\n");
        if (myId != 1) {
            // Guest: 생존자 리스트 출력
            pc.printf("[VOTE] 현재 생존자 ID 목록:\n");
            for (int i = 0; i < NUM_PLAYERS; i++) {
                if (players[i].isAlive) {
                    pc.printf(" - ID %d\n", players[i].id);
                }
            }
            pc.printf("[VOTE] 처형할 플레이어의 ID를 입력하세요: ");
        } else {
            // HOST: guest들에게 투표 시작 신호 전송
            uint8_t signal[] = {'S'};
            for (int i = 0; i < NUM_PLAYERS; i++) {
                if (players[i].id != myId && players[i].isAlive) {
                    L3_LLI_dataReqFunc(signal, 1, players[i].id);
                }
            }
        }
        printedOnce = true;
    }

    // Guest의 투표 입력 처리
    if (myId != 1 && pc.readable()) {
        char c = pc.getc();
        if (c >= '0' && c <= '9') {
            int votedId = c - '0';
            bool valid = false;
            for (int i = 0; i < NUM_PLAYERS; i++) {
                if (players[i].id == votedId && players[i].isAlive) {
                    valid = true;
                    break;
                }
            }
            if (valid) {
                pc.printf("[VOTE] ID %d에게 투표했습니다.\n", votedId);
                char msg[8];
                snprintf(msg, sizeof(msg), "VOTE:%d", votedId);
                L3_LLI_dataReqFunc((uint8_t*)msg, strlen(msg), 1);
                printedOnce = false;
                main_state = WAIT;  // GUEST는 기다리기 상태로 전환 (호스트가 결과 전송할 때까지)
            } else {
                pc.printf("[VOTE] 유효하지 않은 ID입니다. 생존자에게만 투표하세요.\n");
            }
        } else {
            pc.printf("[VOTE] 숫자 ID만 입력 가능합니다.\n");
        }
    }

    // HOST가 투표 수신 및 집계
    if (L3_event_checkEventFlag(L3_event_msgRcvd)) {
        uint8_t* msg = L3_LLI_getMsgPtr();
        uint8_t len = L3_LLI_getSize();

        if (strncmp((char*)msg, "VOTE:", 5) == 0 && myId == 1) {
            int targetId = atoi((char*)msg + 5);
            for (int i = 0; i < NUM_PLAYERS; i++) {
                if (players[i].id == targetId) {
                    voteCounts[i]++;
                    totalVotesReceived++;
                    pc.printf("[HOST] %d번에게 투표 1표 누적\n", targetId);
                }
            }

            // 모든 생존자로부터 투표 받으면 결과 처리
            int aliveCount = 0;
            for (int i = 0; i < NUM_PLAYERS; i++) {
                if (players[i].isAlive && players[i].id != myId) aliveCount++;
            }

            if (totalVotesReceived >= aliveCount) {
                int maxVote = -1;
                int maxIndex = -1;
                for (int i = 0; i < NUM_PLAYERS; i++) {
                    if (voteCounts[i] > maxVote) {
                        maxVote = voteCounts[i];
                        maxIndex = i;
                    }
                }

                if (maxIndex != -1) {
                    int eliminatedId = players[maxIndex].id;
                    players[maxIndex].isAlive = false;
                    pc.printf("[HOST] %d번 플레이어가 처형되었습니다.\n", eliminatedId);

                    // 모두에게 결과 전송
                    char result[16];
                    snprintf(result, sizeof(result), "DEAD:%d", eliminatedId);
                    for (int i = 0; i < NUM_PLAYERS; i++) {
                        if (players[i].id != myId && players[i].isAlive) {
                            L3_LLI_dataReqFunc((uint8_t*)result, strlen(result), players[i].id);
                        }
                    }
                }

                // 다음 상태 전이
                printedOnce = false;
                totalVotesReceived = 0;
                memset(voteCounts, 0, sizeof(voteCounts));
                main_state = NIGHT;
            }
        }
        L3_event_clearEventFlag(L3_event_msgRcvd);
    }
}
