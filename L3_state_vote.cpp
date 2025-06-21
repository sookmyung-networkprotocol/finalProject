#include "L3_shared.h"
#include "L3_FSMevent.h"
#include "L3_LLinterface.h"
#include "mbed.h"
#include <map>

void L3_handleVote()
{
    static bool printed = false;
    static bool voted = false;
    static std::map<uint8_t, int> voteCounts;  // ID별 투표 수 집계
    static int receivedVotes = 0;

    // [0] 게스트가 vote start 신호 'S'를 수신한 경우 상태 전이 (예비 안전용)
    if (myId != 1 && L3_event_checkEventFlag(L3_event_msgRcvd)) {
        uint8_t* msg = L3_LLI_getMsgPtr();
        uint8_t size = L3_LLI_getSize();

        if (size == 1 && msg[0] == 'S') {
            pc.printf("[DEBUG] 투표 시작 신호 수신 → VOTE 상태로 전이\n");
            main_state = VOTE;
        }

        L3_event_clearEventFlag(L3_event_msgRcvd);
    }

    // [1] 안내 메시지 출력 (1회)
    if (!printed) {
        pc.printf("\n[VOTE] 투표를 시작합니다.\n");

        if (myId != 1 && !idead) {
            pc.printf("[VOTE] 현재 생존자 ID 목록: ");
            for (int i = 0; i < NUM_PLAYERS; i++) {
                if (players[i].isAlive && players[i].id != 1) {
                    pc.printf("%d ", players[i].id);
                }
            }
            pc.printf("\n[VOTE] 처형할 플레이어의 ID를 입력하세요: ");
        }

        printed = true;
    }

    // [2] 게스트의 투표 입력
    if (myId != 1 && !idead && !voted) {
        if (pc.readable()) {
            char input = pc.getc();
            pc.putc(input);

            if (input >= '0' && input <= '9') {
                uint8_t targetId = input - '0';
                bool valid = false;
                for (int i = 0; i < NUM_PLAYERS; i++) {
                    if (players[i].id == targetId && players[i].isAlive) {
                        valid = true;
                        break;
                    }
                }

                if (valid) {
                    uint8_t voteMsg[2] = {'V', targetId};
                    L3_LLI_dataReqFunc(voteMsg, 2, 1);  // Host에게 전송
                    pc.printf(" \u2192 %d번에게 투표 전송 완료\n", targetId);
                    voted = true;
                } else {
                    pc.printf("\n[VOTE] 유효하지 않은 ID입니다. 생존자에게만 투표하세요.\n");
                }
            } else {
                pc.printf("\n[VOTE] 숫자 ID만 입력 가능합니다.\n");
            }
        }
    }

    // [3] Host가 수신된 투표 메시지 처리
    if (myId == 1 && L3_event_checkEventFlag(L3_event_msgRcvd)) {
        uint8_t* msg = L3_LLI_getMsgPtr();
        uint8_t size = L3_LLI_getSize();

        if (size == 2 && msg[0] == 'V') {
            uint8_t votedId = msg[1];
            voteCounts[votedId]++;
            receivedVotes++;
            pc.printf("[VOTE] 플레이어의 투표 수신 → ID %d (총 %d표)\n", votedId, voteCounts[votedId]);
        }
        L3_event_clearEventFlag(L3_event_msgRcvd);
    }

    // [4] 모든 생존자로부터 투표 수신 완료 시 처리
    if (myId == 1) {
        int alivePlayers = 0;
        for (int i = 0; i < NUM_PLAYERS; i++) {
            if (players[i].isAlive && players[i].id != 1) // Host 제외
                alivePlayers++;
        }

        if (receivedVotes >= alivePlayers && alivePlayers > 0) {
            // 최대 득표자 계산
            uint8_t maxId = 0;
            int maxVotes = 0;
            for (auto& entry : voteCounts) {
                if (entry.second > maxVotes) {
                    maxVotes = entry.second;
                    maxId = entry.first;
                }
            }

            // 결과 출력 및 반영
            pc.printf("\n[VOTE] 최다 득표자: %d (%d표)\n", maxId, maxVotes);
            for (int i = 0; i < NUM_PLAYERS; i++) {
                if (players[i].id == maxId) {
                    players[i].isAlive = false;
                    if (myId == maxId) idead = true;
                }
            }

            // 다음 상태로 전이
            printed = false;
            voted = false;
            voteCounts.clear();
            receivedVotes = 0;
            main_state = NIGHT;
        }
    }
}
