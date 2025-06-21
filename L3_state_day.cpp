#include "L3_shared.h"
#include "L3_LLinterface.h"
#include "L3_FSMevent.h"
#include "mbed.h"
#include <cstring>
#include <cstdlib>

void L3_handleDay()
{
    static bool announced = false;

    // 안내 메시지 (한 번만 출력)
    if (!announced) {
        pc.printf("\n🌞 [DAY] 낮이 되었습니다. 그룹 채팅을 시작합니다.\n");
        pc.printf("✏️ 메시지를 입력하고 Enter를 누르세요. ('v' 입력 시 투표로 이동)\n");
        announced = true;
    }

    // 죽은 플레이어는 채팅 불가
    if (idead) {
        pc.printf("\n☠️ [DAY] 당신은 사망했습니다. 채팅에 참여할 수 없습니다.\n");
        announced = false;
        main_state = VOTE;
        return;
    }

    // 메시지 전송 처리
    if (L3_event_checkEventFlag(L3_event_dataToSend)) {
        pc.printf("\n🗨️ [나] %s\n", originalWord);

        // 살아있는 모든 다른 플레이어에게 전송 (자기 제외, HOST 제외)
        for (int i = 0; i < NUM_PLAYERS; i++) {
            if (players[i].isAlive && players[i].id != myId && players[i].id != 1) {
                char taggedMsg[128];
                snprintf(taggedMsg, sizeof(taggedMsg), "%d|%s", myId, originalWord);
                L3_LLI_dataReqFunc((uint8_t*)taggedMsg, strlen(taggedMsg), players[i].id);
                pc.printf("[DEBUG] 전송 → ID %d: \"%s\"\n", players[i].id, taggedMsg);
            }
        }

        L3_event_clearEventFlag(L3_event_dataToSend);
    }

    // 수신 메시지 처리
    if (L3_event_checkEventFlag(L3_event_msgRcvd)) {
        uint8_t* msg = L3_LLI_getMsgPtr();
        uint8_t size = L3_LLI_getSize();

        // 1. HOST가 보낸 VOTE 전이 신호인 경우
        if (size == 1 && msg[0] == 'S') {
            pc.printf("\n📩 [SYSTEM] HOST가 투표를 시작했습니다.\n");
            announced = false;
            main_state = VOTE;
            L3_event_clearEventFlag(L3_event_msgRcvd);
            return;
        }

        // 2. 일반 채팅 메시지: "ID|내용" 형식
        int senderId = -1;
        char* sep = (char*)memchr(msg, '|', size);
        if (sep != NULL) {
            *sep = '\0';
            senderId = atoi((char*)msg);
            char* realMsg = sep + 1;
            pc.printf("\n📩 [Player %d] %s\n", senderId, realMsg);
        } else {
            pc.printf("\n📩 [알 수 없음] %.*s\n", size, msg);
        }

        L3_event_clearEventFlag(L3_event_msgRcvd);
    }

    // 'v' 입력 시 VOTE로 전이
    if (pc.readable()) {
        char c = pc.getc();
        if (c == 'v') {
            pc.printf("\n🔚 [DAY] 채팅을 종료하고 투표를 시작합니다.\n");

            if (myId == 1) {
                // HOST가 투표 시작 신호 전송
                uint8_t signal[] = {'S'};
                for (int i = 0; i < NUM_PLAYERS; i++) {
                    if (players[i].id != myId && players[i].isAlive) {
                        L3_LLI_dataReqFunc(signal, 1, players[i].id);
                        pc.printf("[DEBUG] VOTE 신호 전송 → ID %d\n", players[i].id);
                    }
                }
            }

            announced = false;
            main_state = VOTE;
        }
    }
}
