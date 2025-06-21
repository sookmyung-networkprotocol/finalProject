#include "L3_state_vote.h"
#include "L3_host.h"
#include "L3_msg.h"
#include "L3_timer.h"
#include "L3_LLinterface.h"
#include "L3_FSMevent.h"
#include "protocol_parameters.h"
#include "mbed.h"

extern Serial pc;
extern int change_state;
extern uint8_t myId;
extern char myRoleName[16];
extern bool idead;
extern int doctorTarget;
extern bool dead[NUM_PLAYERS];
extern Player players[NUM_PLAYERS];

void L3_handleVote() {
    static int voteDoneCount = 0;
    static int aliveCount = 0;
    static bool gameOver = false;

    #define MAX_PLAYER_ID 9
    static int voteResults[MAX_PLAYER_ID + 1] = {0};
    static bool waitingAck = false;
    static bool waitingHostInput = false;
    static int currentSendIndex = 0;
    static int aliveIDs[NUM_PLAYERS];
    static bool voteCompleted[NUM_PLAYERS] = {false};

    static int maxVotedId = -1;

    static char msgStr[512];

    if (myId == 1) {
        if (change_state == 0) {
            aliveCount = 0;
            voteDoneCount = 0;
            currentSendIndex = 0;
            waitingAck = false;
            waitingHostInput = false;

            for (int i = 0; i < NUM_PLAYERS; i++) {
                if (!dead[i]) {
                    aliveIDs[aliveCount++] = players[i].id;
                }
                players[i].Voted = 0;
                voteCompleted[i] = false;
                voteResults[i] = 0;
            }

            pc.printf("\r\n\U0001F4E3 투표를 시작합니다.\r\n");
            pc.printf("\r\n\U0001F9CD 살아있는 플레이어 목록:\r\n");
            for (int i = 0; i < aliveCount; i++) {
                pc.printf("Player ID: %d ", aliveIDs[i]);
            }
            pc.printf("\r\n-----------------------------------------\r\n");

            change_state = 1;
        }

        if (change_state == 1) {
            if (!waitingAck && !waitingHostInput && currentSendIndex < aliveCount) {
                int destId = aliveIDs[currentSendIndex];
                msgStr[0] = '\0';

                sprintf(msgStr, "투표하세요. 본인을 제외한 ID 중 선택: ");
                for (int i = 0; i < aliveCount; i++) {
                    if (aliveIDs[i] != destId) {
                        char idStr[4];
                        sprintf(idStr, "%d ", aliveIDs[i]);
                        strcat(msgStr, idStr);
                    }
                }

                L3_LLI_dataReqFunc((uint8_t*)msgStr, strlen(msgStr), destId);
                pc.printf("\r\n[Host] %d번 플레이어에게 투표 요청: %s", destId, msgStr);
                waitingAck = true;
            }

            if (L3_event_checkEventFlag(L3_event_msgRcvd)) {
                uint8_t* dataPtr = L3_LLI_getMsgPtr();
                int fromId = L3_LLI_getSrcId();
                int voteTo = atoi((char*)dataPtr);

                if (voteTo >= 0 && voteTo <= MAX_PLAYER_ID) {
                    voteResults[voteTo]++;
                }

                voteDoneCount++;
                voteCompleted[currentSendIndex] = true;

                pc.printf("\r\n\U0001F5F3️ %d번 플레이어가 %d번에게 투표했습니다.", fromId, voteTo);

                waitingAck = false;
                waitingHostInput = true;

                pc.printf("\r\n▶ 다음 플레이어에게 전송하려면 '1'을 입력하세요.");

                L3_event_clearEventFlag(L3_event_msgRcvd);
            }

            if (waitingHostInput && pc.readable()) {
                char c = pc.getc();
                if (c == '1') {
                    currentSendIndex++;
                    waitingHostInput = false;
                    pc.printf("\r\n✅ 다음 플레이어로 이동합니다.");

                    if (currentSendIndex >= aliveCount) {
                        pc.printf("\r\n✅ 모든 투표 완료! 결과:\r\n");
                        for (int i = 0; i < NUM_PLAYERS; i++) {
                            if (players[i].isAlive) {
                                pc.printf("Player %d: %d표\n", players[i].id, voteResults[players[i].id]);
                            }
                        }
                        change_state = 2;
                        currentSendIndex = 0;
                        waitingAck = false;
                        waitingHostInput = false;
                    }
                } else {
                    pc.printf("\r\n❗ '1'을 입력해야 진행됩니다.");
                }
            }
        }
    }
}
