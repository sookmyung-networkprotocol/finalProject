#include "L3_FSMevent.h"
#include "L3_msg.h"
#include "L3_timer.h"
#include "L3_LLinterface.h"
#include "protocol_parameters.h"
#include "mbed.h"
#include "L2_FSMmain.h"
#include "L3_host.h"

int change_state = 0;

// 게임 상태 변수들
static int currentSendIndex = 0;
static bool waitingAck = false;
static char myRoleName[16] = {0};
static bool idead = false;

//FSM state
#define L3STATE_IDLE                0

//state variables
static uint8_t main_state = L3STATE_IDLE;
static uint8_t prev_state = main_state;

//SDU (input)
char msgStr[200];
static uint8_t originalWord[1030];
static uint8_t wordLen=0;
static uint8_t sdu[1030];

//serial port interface
static Serial pc(USBTX, USBRX);
static uint8_t myId;
static uint8_t myDestId;

//application event handler : generating SDU from keyboard input
static void L3service_processInputWord(void)
{
    char c = pc.getc();
    if (!L3_event_checkEventFlag(L3_event_dataToSend))
    {
        if (c == '\n' || c == '\r')
        {
            originalWord[wordLen++] = '\0';
            L3_event_setEventFlag(L3_event_dataToSend);
            debug_if(DBGMSG_L3,"word is ready! ::: %s\n", originalWord);
        }
        else
        {
            originalWord[wordLen++] = c;
            if (wordLen >= L3_MAXDATASIZE-1)
            {
                originalWord[wordLen++] = '\0';
                L3_event_setEventFlag(L3_event_dataToSend);
                pc.printf("\n max reached! word forced to be ready :::: %s\n", originalWord);
            }
        }
    }
}

void L3_initFSM(uint8_t Id, uint8_t destId)
{
    myId = Id;
    myDestId = destId;
    
    // 호스트 여부 판단 (ID 1이 호스트)
    isHost = (myId == 1);
    isInTeam = false;
    playerCnt = 0;
    rcvdCnfCnt = 0;
    
    // 키보드 입력 이벤트 핸들러 등록
    pc.attach(&L3service_processInputWord, Serial::RxIrq);
    
    if (isHost) {
        pc.printf("Host mode initialized. Waiting for players...\n");
    } else {
        pc.printf("Player mode initialized. Waiting for match...\n");
    }
}

void L3_FSMrun(void)
{   
    if (prev_state != main_state)
    {
        debug_if(DBGMSG_L3, "[L3] State transition from %i to %i\n", prev_state, main_state);
        prev_state = main_state;
    }

    //FSM implementation
    switch (main_state)
    {
        case L3STATE_IDLE: // IDLE state
        {
            // Host 초기화 및 매치 요청 전송
            if (isHost && !isInTeam && change_state == 0) 
            {
                pc.printf("Starting match requests...\n");
                // 매치 요청 전송 로직 (여기서는 단순화)
                change_state = 1;
                main_state = MATCH;
            }
            // Player 매치 요청 수신 대기
            else if (!isHost && !isInTeam)
            {
                if (L3_event_checkEventFlag(L3_event_msgRcvd))
                {
                    uint8_t* dataPtr = L3_LLI_getMsgPtr();
                    if (dataPtr[0] == MSG_TYPE_MATCH_REQ)
                    {
                        // 매치 ACK 전송
                        uint8_t ackMsg[2] = {MSG_TYPE_MATCH_ACK, 0};
                        L3_LLI_dataReqFunc(ackMsg, 2, 1); // Host에게 전송
                        pc.printf("Match ACK sent to host\n");
                    }
                    L3_event_clearEventFlag(L3_event_msgRcvd);
                }
            }
            
            // 일반 메시지 처리
            if (L3_event_checkEventFlag(L3_event_msgRcvd))
            {
                uint8_t* dataPtr = L3_LLI_getMsgPtr();
                uint8_t size = L3_LLI_getSize();
                
                // 그룹 채팅 메시지 처리
                if (dataPtr[0] == MSG_TYPE_GC)
                {
                    pc.printf("\r\n[GROUP CHAT] %.*s\n\n", size-1, dataPtr+1);
                }
                
                L3_event_clearEventFlag(L3_event_msgRcvd);
            }
            
            // 키보드 입력으로 그룹 채팅 전송
            if (L3_event_checkEventFlag(L3_event_dataToSend) && idead == false)
            {
                // 그룹 채팅 메시지 구성
                sdu[0] = MSG_TYPE_GC;
                strcpy((char*)sdu+1, (char*)originalWord);
                
                // 모든 살아있는 플레이어에게 전송 (브로드캐스트)
                L3_LLI_dataReqFunc(sdu, wordLen+1, 255); // 255는 브로드캐스트
                
                wordLen = 0;
                L3_event_clearEventFlag(L3_event_dataToSend);
            }
            
            break;
        }
            
        case MATCH: // 매치메이킹 상태
        {
            if (isHost)
            {
                // Host: 플레이어들로부터 ACK 수신 대기
                if (L3_event_checkEventFlag(L3_event_msgRcvd))
                {
                    uint8_t* dataPtr = L3_LLI_getMsgPtr();
                    if (dataPtr[0] == MSG_TYPE_MATCH_ACK)
                    {
                        playerCnt++;
                        pc.printf("Player %d joined. Total: %d/3\n", L3_LLI_getSrcId(), playerCnt);
                        
                        // CNF 전송
                        uint8_t cnfMsg[2] = {MSG_TYPE_MATCH_CNF, 0};
                        L3_LLI_dataReqFunc(cnfMsg, 2, L3_LLI_getSrcId());
                        
                        if (playerCnt >= 3) // 3명 이상 모이면 게임 시작
                        {
                            isInTeam = true;
                            main_state = MODE_1;
                            change_state = 0;
                        }
                    }
                    L3_event_clearEventFlag(L3_event_msgRcvd);
                }
            }
            else
            {
                // Player: CNF 수신 대기
                if (L3_event_checkEventFlag(L3_event_msgRcvd))
                {
                    uint8_t* dataPtr = L3_LLI_getMsgPtr();
                    if (dataPtr[0] == MSG_TYPE_MATCH_CNF)
                    {
                        isInTeam = true;
                        pc.printf("Match confirmed! Waiting for game start...\n");
                    }
                    L3_event_clearEventFlag(L3_event_msgRcvd);
                }
            }
            break;
        }
        
        case MODE_1: // 역할 배정 단계
        {
            static bool waitingAck = false;
            static bool waitingHostInput = false;
            static int currentSendIndex = 0;
            static char msgStr[64]; 

            // 시작점 - 호스트이면 역할 배정 
            if (myId == 1 && change_state == 0) 
            { 
                createPlayers();

                for (int i = 0; i < 4; i++) {
                    pc.printf("\r\nPlayer %d - ID: %d, Role: %s\n\n", 
                            i, players[i].id, getRoleName(players[i].role));
                }

                currentSendIndex = 0;
                waitingAck = false;
                waitingHostInput = false;
                change_state = 1;
            } 
            
            // 호스트: 역할 전송 
            if (myId == 1 && change_state == 1) {
                if (!waitingAck && !waitingHostInput && currentSendIndex < 4) {
                    int destId = players[currentSendIndex].id;
                    
                    // 역할 메시지 구성
                    msgStr[0] = MSG_TYPE_ROLE_ASSIGN;
                    strcpy(msgStr+1, getRoleName(players[currentSendIndex].role));
                    
                    pc.printf("\r\nSEND ROLE to ID %d : %s\n\n", destId, msgStr+1);
                    
                    L3_LLI_dataReqFunc((uint8_t*)msgStr, strlen(msgStr), destId);
                    waitingAck = true;
                }
                
                if (L3_event_checkEventFlag(L3_event_msgRcvd)) {
                    uint8_t* dataPtr = L3_LLI_getMsgPtr();
                    uint8_t size = L3_LLI_getSize();
                    
                    if (size == 3 && strncmp((char*)dataPtr, "ACK", 3) == 0 && waitingAck) {
                        pc.printf("\r\nACK received from player ID %d\n", L3_LLI_getSrcId());
                        waitingAck = false;
                        currentSendIndex++;

                        if (currentSendIndex >= 4) {
                            pc.printf("\r\n게임이 시작되었습니다.\n\n");
                            change_state = 2;
                        }
                    }

                    L3_event_clearEventFlag(L3_event_msgRcvd);
                }
            }

            // 게스트: 메시지 수신 이벤트 처리
            if (myId != 1 && L3_event_checkEventFlag(L3_event_msgRcvd)) {
                uint8_t* dataPtr = L3_LLI_getMsgPtr();
                uint8_t size = L3_LLI_getSize();

                if (dataPtr[0] == MSG_TYPE_ROLE_ASSIGN) {
                    int len = size - 1;
                    if (len > 15) len = 15;
                    memcpy(myRoleName, dataPtr+1, len);
                    myRoleName[len] = '\0';

                    pc.printf("\r\n나의 역할 : %s\n\n", myRoleName);

                    // ACK 전송
                    const char ackMsg[] = "ACK";
                    L3_LLI_dataReqFunc((uint8_t*)ackMsg, sizeof(ackMsg) - 1, 1);

                    pc.printf("\r\n게임이 시작되었습니다.\n\n");
                    change_state = 2;
                }
                
                L3_event_clearEventFlag(L3_event_msgRcvd);
            }

            // 조건부 상태 전환
            if (change_state == 2)
                main_state = DAY;

            break;
        }

        case DAY: // 낮 단계 (그룹 채팅)
        {
            static bool dayStarted = false;
            
            if (!dayStarted) {
                pc.printf("\r\n=== 낮이 되었습니다. 자유롭게 대화하세요 ===\n\n");
                dayStarted = true;
                
                // 호스트는 일정 시간 후 투표 시작
                if (isHost) {
                    L3_timer_startTimer(); // 타이머 시작
                }
            }
            
            // 타이머 만료 시 투표 단계로 전환 (호스트만)
            if (isHost && L3_timer_getTimerStatus() == 0) {
                dayStarted = false;
                main_state = VOTE;
                change_state = 0;
                pc.printf("\r\n=== 투표 시간입니다 ===\n\n");
            }
            
            break;
        }
        
        case VOTE: // 투표 단계
        {
            // 기존 VOTE 로직 유지 (이미 구현되어 있음)
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
            static char msgStr[512];
            
            // 호스트: 투표 초기화 및 진행
            if (myId == 1 && change_state == 0) {
                aliveCount = 0;
                voteDoneCount = 0;
                currentSendIndex = 0;
                waitingAck = false;
                waitingHostInput = false;

                for (int i = 0; i < NUM_PLAYERS; i++) {
                    if (players[i].isAlive) {
                        aliveIDs[aliveCount++] = players[i].id;
                    }
                    players[i].Voted = 0;
                    voteCompleted[i] = false;
                    if (players[i].id <= MAX_PLAYER_ID) {
                        voteResults[players[i].id] = 0;
                    }
                }

                pc.printf("\r\n📢 투표를 시작합니다.\r\n");
                change_state = 1;
            }

//////////////////////////////////////////////////////////////
            // 2. 투표 메시지 전송 단계 (호스트)
            if (myId == 1 && change_state == 1) {
                // 플레이어에게 투표 요청 메시지 전송
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

                // 플레이어로부터 투표 응답 수신 처리
                if (L3_event_checkEventFlag(L3_event_msgRcvd)) {
                    uint8_t* dataPtr = L3_LLI_getMsgPtr();
                    int fromId = L3_LLI_getSrcId();
                    int voteTo = atoi((char*)dataPtr);

                    // 투표 집계 
                    if (voteTo >= 0 && voteTo <= MAX_PLAYER_ID) {
                        voteResults[voteTo]++;
                    }


                    voteDoneCount++;
                    voteCompleted[currentSendIndex] = true;

                    pc.printf("\r\n🗳️ %d번 플레이어가 %d번에게 투표했습니다.", fromId, voteTo);

                    waitingAck = false;
                    waitingHostInput = true;

                    pc.printf("\r\n▶ 다음 플레이어에게 전송하려면 '1'을 입력하세요.");

                    L3_event_clearEventFlag(L3_event_msgRcvd);
                }

                // 호스트가 1 입력하면 다음 플레이어로 진행
                if (waitingHostInput && pc.readable()) {
                    char c = pc.getc();
                    if (c == '1') {
                        currentSendIndex++;
                        waitingHostInput = false;
                        pc.printf("\r\n✅ 다음 플레이어로 이동합니다.");

                        // 모든 플레이어 투표 완료 시 상태 전환
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

            // 3. 게스트 측: 투표 요청 메시지 수신 → 투표 입력 → 전송
            if (myId != 1 && change_state < 2 && L3_event_checkEventFlag(L3_event_msgRcvd)) {
                uint8_t* dataPtr = L3_LLI_getMsgPtr();
                uint8_t size = L3_LLI_getSize();

                pc.printf("\r\n📨 투표 메시지 수신: %.*s", size, dataPtr);

                // 유효한 투표 대상 ID 파싱
                int validIDs[NUM_PLAYERS];
                int validIDCount = 0;
                for (int i = 0; i < size; i++) {
                    if (dataPtr[i] >= '0' && dataPtr[i] <= '9') {
                        int id = dataPtr[i] - '0';
                        if (id != myId) {
                            validIDs[validIDCount++] = id;
                        }
                    }
                }

                int valid = 0;
                int voteTo = -1;

                while (!valid) {
                    pc.printf("\r\n📝 투표할 플레이어 ID를 입력하세요: ");
                    while (!pc.readable());
                    char ch = pc.getc();
                    pc.printf("%c", ch);

                    if (ch < '0' || ch > '9') {
                        pc.printf("\r\n❗ 숫자가 아닙니다. 다시 입력하세요.");
                        continue;
                    }

                    voteTo = ch - '0';

                    if (voteTo == myId) {
                        pc.printf("\r\n❗ 자신에게는 투표할 수 없습니다.");
                        continue;
                    }

                    bool isValid = false;
                    for (int i = 0; i < validIDCount; i++) {
                        if (validIDs[i] == voteTo) {
                            isValid = true;
                            break;
                        }
                    }

                    if (!isValid) {
                        pc.printf("\r\n❗ 해당 ID는 유효한 투표 대상이 아닙니다. 다시 입력하세요.");
                        continue;
                    }

                    valid = 1;
                }

                // 투표 결과 전송 (Host = 1)
                char ackMsg[4];
                sprintf(ackMsg, "%d", voteTo);
                L3_LLI_dataReqFunc((uint8_t*)ackMsg, strlen(ackMsg), 1);
                L3_event_clearEventFlag(L3_event_msgRcvd);

                change_state = 2;
            }

            // 4. 투표 결과 전송 단계 (호스트)
            if (myId == 1 && change_state == 2) {
                static char msgStr[512];
                static bool msgGenerated = false;
                static int currentSendIndex = 0;
                static bool waitingAck = false;
                static bool waitingHostInput = false;
                static int killedByVoteId = -1; // 투표로 죽은 플레이어 ID 저장

                if (!msgGenerated) {
                    // 메시지 구성 시작
                    msgStr[0] = '\0';
                    strcat(msgStr, "투표 결과: ");

                    for (int i = 0; i < NUM_PLAYERS; i++) {
                        if (players[i].isAlive) {
                            char buf[32];
                            sprintf(buf, "[%d: %d표] ", players[i].id, voteResults[players[i].id]);
                            strcat(msgStr, buf);
                        }
                    }

                    // 최다 득표자 계산
                    int maxVotes = 0;
                    int maxVotedId = -1;
                    bool tie = false;
                    for (int i = 0; i < NUM_PLAYERS; i++) {
                        int id = players[i].id;
                        if (!players[i].isAlive) continue;

                        if (voteResults[id] > maxVotes) {
                            maxVotes = voteResults[id];
                            maxVotedId = id;
                            tie = false;
                        } else if (voteResults[id] == maxVotes && id != maxVotedId && maxVotes > 0) {
                            tie = true;
                        }
                    }

                    // 투표 결과 메시지 추가 및 실제 플레이어 제거
                    if (!tie && maxVotedId != -1) {
                        strcat(msgStr, "\n💀 ");
                        char killBuf[64];
                        sprintf(killBuf, "%d번 플레이어가 처형되었습니다.", maxVotedId);
                        strcat(msgStr, killBuf);

                        // ⭐ 핵심 수정: 실제로 players 배열에서 플레이어를 죽음 처리
                        killedByVoteId = maxVotedId;
                        for (int i = 0; i < NUM_PLAYERS; i++) {
                            if (players[i].id == maxVotedId) {
                                players[i].isAlive = false;
                                pc.printf("[HOST] 투표 결과: %d번 플레이어 처형 처리 완료\n", maxVotedId);
                                break;
                            }
                        }
                    } else {
                        strcat(msgStr, "\n⚖️ 동점으로 아무도 죽지 않았습니다.");
                        killedByVoteId = -1;
                    }

                    // 생존 마피아/시민 수 계산 (투표 결과 반영 후)
                    int num_mafia = 0;
                    int num_citizen = 0;
                    for (int i = 0; i < NUM_PLAYERS; i++) {
                        if (!players[i].isAlive) continue;

                        if (players[i].role == ROLE_MAFIA)
                            num_mafia++;
                        else
                            num_citizen++;
                    }

                    // 게임 종료 여부 판단
                    if (num_mafia == 0) {
                        strcat(msgStr, "\n🎉 시민 승리! 게임 종료.");
                        gameOver = true;
                    } else if (num_citizen <= num_mafia) {
                        strcat(msgStr, "\n💀 마피아 승리! 게임 종료.");
                        gameOver = true;
                    } else {
                        strcat(msgStr, "\n🌙 밤으로 넘어갑니다.");
                    }

                    msgGenerated = true;
                }

                // 메시지 전송 로직 (기존과 동일)
                if (!waitingAck && !waitingHostInput && currentSendIndex < aliveCount) {
                    int destId = aliveIDs[currentSendIndex];
                    L3_LLI_dataReqFunc((uint8_t*)msgStr, strlen(msgStr), destId);
                    pc.printf("\n[Host] %d번 플레이어에게 투표 결과 전송\n", destId);
                    waitingAck = true;
                }

                // ACK 수신 및 다음 플레이어 전송 로직 (기존과 동일)
                if (L3_event_checkEventFlag(L3_event_msgRcvd)) {
                    uint8_t* dataPtr = L3_LLI_getMsgPtr();
                    uint8_t size = L3_LLI_getSize();

                    if (size == 3 && strncmp((char*)dataPtr, "ACK", 3) == 0 && waitingAck) {
                        pc.printf("ACK 수신됨 (플레이어 ID: %d)\n", aliveIDs[currentSendIndex]);
                        waitingAck = false;
                        waitingHostInput = true;
                        pc.printf("다음 플레이어에게 전송하려면 '1'을 입력하세요:\n");
                    }

                    L3_event_clearEventFlag(L3_event_msgRcvd);
                }

                if (waitingHostInput && pc.readable()) {
                    char c = pc.getc();
                    if (c == '1') {
                        currentSendIndex++;
                        waitingHostInput = false;
                        pc.printf("✅ 다음 플레이어로 이동합니다.\n");

                        if (currentSendIndex >= aliveCount) {
                            pc.printf("✅ 모든 투표 결과 전송 완료!\n");
                            
                            // ⭐ 디버깅: 현재 살아있는 플레이어 목록 출력
                            pc.printf("=== 투표 후 생존자 목록 ===\n");
                            for (int i = 0; i < NUM_PLAYERS; i++) {
                                pc.printf("Player %d (ID:%d): %s\n", i, players[i].id, 
                                        players[i].isAlive ? "ALIVE" : "DEAD");
                            }
                            pc.printf("========================\n");
                            
                            msgGenerated = false;
                            currentSendIndex = 0;
                            change_state = 3;
                        }
                    }
                }
            }


            // 6. 모두 상태 전환: 투표 종료 시 밤(야간) 효과 적용
            if (change_state == 3) {
                // 🌙 밤에 마피아가 선택한 타겟 적용
                int mafiaTarget = -1;
                for (int i = 0; i < NUM_PLAYERS; i++) {
                    if (players[i].role == ROLE_MAFIA && players[i].isAlive) {
                        mafiaTarget = players[i].sentVoteId;
                        break;
                    }
                }

                // 🩺 의사 보호 효과 적용
                bool doctorProtected = false;
                bool someoneKilledByMafia = false;
                
                if (mafiaTarget != -1) {
                    // 의사가 보호한 대상과 마피아 타겟 비교
                    if (mafiaTarget == doctorTarget) {
                        // 보호 성공!
                        doctorProtected = true;
                        pc.printf("🛡️ 의사가 %d번 플레이어를 보호했습니다! 마피아의 공격이 실패했습니다.\n", mafiaTarget);
                    } else {
                        // 보호하지 못함, 마피아 타겟 죽음
                        for (int i = 0; i < NUM_PLAYERS; i++) {
                            if (players[i].id == mafiaTarget && players[i].isAlive) {
                                players[i].isAlive = false;
                                someoneKilledByMafia = true;
                                pc.printf("💀 밤에 %d번 플레이어가 마피아에게 죽었습니다.\n", mafiaTarget);
                                break;
                            }
                        }
                    }
                } else {
                    // 마피아가 아무도 선택하지 않았거나 죽었음
                    pc.printf("🌙 평화로운 밤이었습니다. (마피아 공격 없음)\n");
                }

                // 📊 밤 결과 요약 출력
                if (mafiaTarget != -1) {
                    pc.printf("\n=== 밤 결과 요약 ===\n");
                    pc.printf("🔴 마피아 타겟: %d번\n", mafiaTarget);
                    if (doctorTarget != -1) {
                        pc.printf("🩺 의사 보호: %d번\n", doctorTarget);
                    } else {
                        pc.printf("🩺 의사 보호: 없음 (의사 사망 또는 미선택)\n");
                    }
                    
                    if (doctorProtected) {
                        pc.printf("✅ 결과: 보호 성공! 아무도 죽지 않음\n");
                    } else if (someoneKilledByMafia) {
                        pc.printf("❌ 결과: %d번 플레이어 사망\n", mafiaTarget);
                    }
                    pc.printf("==================\n\n");
                }

                // 🔁 idead 동기화 (자기 자신이 죽었을 경우)
                for (int i = 0; i < NUM_PLAYERS; i++) {
                    if (players[i].id == myId && !players[i].isAlive) {
                        idead = true;
                        pc.printf("❗ 당신은 죽었습니다.\n");
                    }
                }

                // 🔄 밤 단계 변수 초기화
                for (int i = 0; i < NUM_PLAYERS; i++) {
                    players[i].sentVoteId = -1;
                }
                doctorTarget = -1;

                // 🧹 투표 관련 변수 초기화
                voteDoneCount = 0;
                for (int i = 0; i < NUM_PLAYERS; i++) {
                    if (players[i].id <= MAX_PLAYER_ID) {
                        voteResults[players[i].id] = 0;
                    }
                    voteCompleted[i] = false;
                    players[i].Voted = 0;
                }

                // 📈 게임 종료 조건 재검사 (밤 효과 적용 후)
                int num_mafia = 0;
                int num_citizen = 0;
                for (int i = 0; i < NUM_PLAYERS; i++) {
                    if (!players[i].isAlive) continue;

                    if (players[i].role == ROLE_MAFIA)
                        num_mafia++;
                    else
                        num_citizen++;
                }

                // 🏁 게임 종료 여부 재판단
                if (num_mafia == 0) {
                    pc.printf("🎉 모든 마피아가 제거되었습니다! 시민 승리!\n");
                    gameOver = true;
                    main_state = OVER;
                } else if (num_citizen <= num_mafia) {
                    pc.printf("💀 마피아가 시민과 같거나 많아졌습니다! 마피아 승리!\n");
                    gameOver = true;
                    main_state = OVER;
                } else {
                    // 게임 계속, 다음 날로
                    pc.printf("☀️ 새로운 날이 밝았습니다.\n");
                    
                    // 🔄 상태 전환 준비
                    change_state = 0;

                    if (myId == 1) {
                        main_state = DAY;  // 호스트는 다음 날 시작
                    } else {
                        // 게스트는 생존 여부에 따라 분기
                        if (!idead) {
                            main_state = DAY;
                        } else {
                            main_state = NIGHT;  // 죽은 플레이어는 관전
                        }
                    }
                }

                // 📊 현재 생존자 목록 출력 (디버깅용)
                pc.printf("\n=== 현재 생존자 목록 ===\n");
                for (int i = 0; i < NUM_PLAYERS; i++) {
                    pc.printf("Player %d (ID:%d): %s - %s\n", 
                            i, players[i].id, 
                            players[i].isAlive ? "ALIVE" : "DEAD",
                            getRoleName(players[i].role));
                }
                pc.printf("========================\n\n");
            }


            break;
        }

        
        case NIGHT: // 밤 단계 (대기)
        {
            pc.printf("\r\n밤이 되었습니다. 특수 역할들이 행동합니다.\n\n");
            
            if (isHost) {
                main_state = MAFIA;
                change_state = 0;
            } else {
                // 플레이어는 역할에 따라 분기
                if (!idead) {
                    if (strcmp(myRoleName, "Mafia") == 0)
                        main_state = MAFIA;
                    else if (strcmp(myRoleName, "Police") == 0)
                        main_state = POLICE;
                    else if (strcmp(myRoleName, "Doctor") == 0)
                        main_state = DOCTOR;
                    else
                        main_state = NIGHT; // 시민은 대기
                }
            }
            break;
        }

        

    case POLICE:
    {
        static bool sentToPolice = false;
        static bool waitingAck = false;
        static int policeId = -1;
        static bool policePhaseComplete = false;

        // 1. Host: 살아있는 경찰에게 메시지 전송
        if (myId == 1 && change_state == 0) {
            policePhaseComplete = false;
            
            // 살아있는 경찰 찾기
            for (int i = 0; i < NUM_PLAYERS; i++) {
                if (players[i].role == ROLE_POLICE && players[i].isAlive) {
                    policeId = players[i].id;
                    break;
                }
            }

            if (policeId == -1) {
                pc.printf("[HOST] 살아있는 경찰 없음. DAY로 이동\n");
                policePhaseComplete = true;
                change_state = 3; // 바로 DAY로 전환
            } else {
                char msg[100] = "정체를 확인할 ID를 입력하세요:";
                for (int i = 0; i < NUM_PLAYERS; i++) {
                    if (players[i].isAlive && players[i].id != policeId) {
                        char buf[5];
                        sprintf(buf, " %d", players[i].id);
                        strcat(msg, buf);
                    }
                }

                L3_LLI_dataReqFunc((uint8_t*)msg, strlen(msg), policeId);
                pc.printf("[HOST] %d번 경찰에게 정체 확인 요청 전송\n", policeId);
                sentToPolice = true;
                waitingAck = true;
                change_state = 1;
            }
        }

        // 2. Host: 경찰 응답 처리 → 정체 전송 → DAY 전환 신호 전송
        if (myId == 1 && change_state == 1 && L3_event_checkEventFlag(L3_event_msgRcvd)) {
            uint8_t* dataPtr = L3_LLI_getMsgPtr();
            int targetId = atoi((char*)dataPtr);
            pc.printf("[HOST] 경찰이 %d번을 선택했습니다.\n", targetId);

            const char* roleStr = "Unknown";
            for (int i = 0; i < NUM_PLAYERS; i++) {
                if (players[i].id == targetId) {
                    roleStr = getRoleName(players[i].role);
                    break;
                }
            }

            // 정체 정보 전송
            L3_LLI_dataReqFunc((uint8_t*)roleStr, strlen(roleStr), policeId);
            pc.printf("[HOST] %d번의 정체 '%s'를 %d번 경찰에게 전송 완료\n", targetId, roleStr, policeId);

            L3_event_clearEventFlag(L3_event_msgRcvd);
            
            // 경찰 단계 완료 - 모든 플레이어에게 DAY 전환 신호 전송
            policePhaseComplete = true;
            change_state = 2;
        }

        // 3. Host: 모든 플레이어에게 DAY 전환 신호 전송
        if (myId == 1 && change_state == 2 && policePhaseComplete) {
            char dayMsg[] = "POLICE_PHASE_END";
            
            // 모든 살아있는 플레이어에게 전송
            for (int i = 0; i < NUM_PLAYERS; i++) {
                if (players[i].isAlive && players[i].id != myId) {
                    L3_LLI_dataReqFunc((uint8_t*)dayMsg, strlen(dayMsg), players[i].id);
                }
            }
            
            pc.printf("🌤️ POLICE 단계 종료. 모든 플레이어에게 DAY 전환 신호 전송\n");
            change_state = 3;
        }

        // 4. Guest: 경찰이 ID 입력 및 전송
        if (myId != 1 && strcmp(myRoleName, "Police") == 0 && !idead &&
            L3_event_checkEventFlag(L3_event_msgRcvd) && change_state == 0)
        {
            uint8_t* dataPtr = L3_LLI_getMsgPtr();
            uint8_t size = L3_LLI_getSize();
            
            // DAY 전환 신호 확인
            if (strncmp((char*)dataPtr, "POLICE_PHASE_END", 16) == 0) {
                pc.printf("[Police] DAY 전환 신호 수신\n");
                change_state = 3;
                L3_event_clearEventFlag(L3_event_msgRcvd);
                break;
            }
            
            pc.printf("[Police] 메시지 수신: %.*s\n", size, dataPtr);

            int inputId = -1;
            bool valid = false;
            while (!valid) {
                pc.printf("[Police] 확인할 ID 입력: ");
                while (!pc.readable());
                char ch = pc.getc();
                pc.printf("%c", ch);

                if (ch >= '0' && ch <= '9') {
                    inputId = ch - '0';
                    valid = true;
                } else {
                    pc.printf("\n❗ 숫자만 입력하세요.");
                }
            }

            char reply[4];
            sprintf(reply, "%d", inputId);
            L3_LLI_dataReqFunc((uint8_t*)reply, strlen(reply), 1);
            pc.printf("[Police] Host에 정체 확인 요청 전송 완료\n");

            L3_event_clearEventFlag(L3_event_msgRcvd);
            change_state = 1;
        }

        // 5. Guest: 경찰이 정체 응답 수신
        if (myId != 1 && strcmp(myRoleName, "Police") == 0 && !idead &&
            L3_event_checkEventFlag(L3_event_msgRcvd) && change_state == 1)
        {
            uint8_t* dataPtr = L3_LLI_getMsgPtr();
            uint8_t size = L3_LLI_getSize();
            
            // DAY 전환 신호 확인
            if (strncmp((char*)dataPtr, "POLICE_PHASE_END", 16) == 0) {
                pc.printf("[Police] DAY 전환 신호 수신\n");
                change_state = 3;
                L3_event_clearEventFlag(L3_event_msgRcvd);
                break;
            }
            
            pc.printf("[Police] 수신된 정체: %.*s\n", size, dataPtr);
            L3_event_clearEventFlag(L3_event_msgRcvd);
            change_state = 2; // 정체 확인 완료, DAY 전환 대기
        }

        // 6. Guest: 경찰이 아닌 플레이어들의 DAY 전환 신호 수신
        if (myId != 1 && (strcmp(myRoleName, "Police") != 0 || idead) &&
            L3_event_checkEventFlag(L3_event_msgRcvd))
        {
            uint8_t* dataPtr = L3_LLI_getMsgPtr();
            
            if (strncmp((char*)dataPtr, "POLICE_PHASE_END", 16) == 0) {
                pc.printf("[Player] DAY 전환 신호 수신\n");
                change_state = 3;
            }
            
            L3_event_clearEventFlag(L3_event_msgRcvd);
        }

        // 7. 모든 플레이어: DAY 상태로 전환
        if (change_state == 3) {
            pc.printf("🌤️ POLICE 단계 종료 → DAY로 이동\n");
            main_state = DAY;
            change_state = 0;
            
            // 정적 변수 초기화
            sentToPolice = false;
            waitingAck = false;
            policeId = -1;
            policePhaseComplete = false;
        }

        break;
    }

        case DOCTOR:
        {
            static bool sentToDoctor = false;
            static bool waitingAck = false;
            static int doctorId = -1;

            // 1. Host: 살아있는 의사에게 메시지 전송
            if (myId == 1 && change_state == 0) {
                doctorTarget = -1;

                for (int i = 0; i < NUM_PLAYERS; i++) {
                    if (players[i].role == ROLE_DOCTOR && players[i].isAlive) {
                        doctorId = players[i].id;
                        break;
                    }
                }

                if (doctorId == -1) {
                    pc.printf("👨‍⚕️ 살아있는 의사 없음. 경찰 단계로 넘어갑니다.\n");
                    change_state = 2;
                } else {
                    // 살아있는 플레이어 ID 목록 구성
                    char msg[100] = "살릴 사람의 ID를 입력하세요:";
                    for (int i = 0; i < NUM_PLAYERS; i++) {
                        if (players[i].isAlive && players[i].id != doctorId) {
                            char buffer[5];
                            sprintf(buffer, " %d", players[i].id);
                            strcat(msg, buffer);
                        }
                    }
                    L3_LLI_dataReqFunc((uint8_t*)msg, strlen(msg), doctorId);
                    pc.printf("[HOST] %d번 의사에게 살릴 ID 요청 전송\n", doctorId);
                    sentToDoctor = true;
                    waitingAck = true;
                    change_state = 1;
                }
            }

            // 2. Host: 의사의 응답 수신
            if (myId == 1 && change_state == 1 && L3_event_checkEventFlag(L3_event_msgRcvd)) {
                uint8_t* dataPtr = L3_LLI_getMsgPtr();
                doctorTarget = atoi((char*)dataPtr);
                pc.printf("🩺 의사가 %d번을 살리기로 선택했습니다.\n", doctorTarget);
                L3_event_clearEventFlag(L3_event_msgRcvd);
                change_state = 2;
            }

            // 3. 게스트: 의사일 경우 메시지 수신 → ID 입력 → 전송
            if (myId != 1 && strcmp(myRoleName, "Doctor") == 0 && !idead &&
                L3_event_checkEventFlag(L3_event_msgRcvd) && change_state == 0)
            {
                uint8_t* dataPtr = L3_LLI_getMsgPtr();
                uint8_t size = L3_LLI_getSize();
                pc.printf("[의사] 메시지 수신: %.*s\n", size, dataPtr);

                int inputId = -1;
                bool valid = false;
                while (!valid) {
                    pc.printf("[의사] 살릴 ID 입력: ");
                    while (!pc.readable());
                    char ch = pc.getc();
                    pc.printf("%c", ch);

                    if (ch >= '0' && ch <= '9') {
                        inputId = ch - '0';
                        valid = true;
                    } else {
                        pc.printf("\n❗ 숫자만 입력하세요.");
                    }
                }

                char reply[4];
                sprintf(reply, "%d", inputId);
                L3_LLI_dataReqFunc((uint8_t*)reply, strlen(reply), 1);
                pc.printf("[의사] %d번을 살리기로 선택하고 Host에 전송 완료\n", inputId);
                L3_event_clearEventFlag(L3_event_msgRcvd);
                change_state = 2;
            }

            // 4. 상태 전환: POLICE 단계로 이동
            if (change_state == 2) {
                main_state = POLICE;
                change_state = 0;
            }

            break;
        }



        case MAFIA:
        {
            static bool waitingAck = false;
            static int mafiaId = -1;
            static bool mafiaMessageSent = false;
            static int aliveIDs[NUM_PLAYERS];
            static int aliveCount = 0;

            // 1. Host: 마피아에게 메시지 전송
            if (myId == 1 && change_state == 0) {
                // 살아있는 마피아 찾기
                mafiaId = -1;
                aliveCount = 0;

                for (int i = 0; i < NUM_PLAYERS; i++) {
                    if (!players[i].isAlive) continue;
                    aliveIDs[aliveCount++] = players[i].id;

                    if (players[i].role == ROLE_MAFIA) {
                        mafiaId = players[i].id;
                    }
                }

                if (mafiaId == -1) {
                    pc.printf("[HOST] 살아있는 마피아 없음. DOCTOR 단계로 넘어감.\n");
                    main_state = DOCTOR;
                    break;
                }

                // 메시지 구성 및 전송
                char msgStr[64] = "죽일 ID를 선택하세요: ";
                for (int i = 0; i < aliveCount; i++) {
                    if (aliveIDs[i] != mafiaId) {
                        char idStr[4];
                        sprintf(idStr, "%d ", aliveIDs[i]);
                        strcat(msgStr, idStr);
                    }
                }

                L3_LLI_dataReqFunc((uint8_t*)msgStr, strlen(msgStr), mafiaId);
                pc.printf("[HOST] %d번 마피아에게 메시지 전송: %s\n", mafiaId, msgStr);

                waitingAck = true;
                mafiaMessageSent = true;
                change_state = 1;
            }

            // 2. Host: 마피아 응답 수신 처리
            if (myId == 1 && change_state == 1 && L3_event_checkEventFlag(L3_event_msgRcvd)) {
                uint8_t* dataPtr = L3_LLI_getMsgPtr();
                int selectedId = atoi((char*)dataPtr);

                pc.printf("[HOST] 마피아가 %d번을 선택했습니다.\n", selectedId);

                // 마피아 타겟 저장
                for (int i = 0; i < NUM_PLAYERS; i++) {
                    if (players[i].id == mafiaId) {
                        players[i].sentVoteId = selectedId;
                        break;
                    }
                }

                L3_event_clearEventFlag(L3_event_msgRcvd);
                change_state = 2;
            }

            // 3. Guest(마피아): 타겟 입력 및 전송
            if (myId != 1 && change_state == 0 &&
                L3_event_checkEventFlag(L3_event_msgRcvd) &&
                strcmp(myRoleName, "Mafia") == 0 && !idead)
            {
                uint8_t* dataPtr = L3_LLI_getMsgPtr();
                uint8_t size = L3_LLI_getSize();

                pc.printf("[Mafia] 타겟 선택 메시지 수신: %.*s\n", size, dataPtr);

                // 유효 ID 파싱
                int validIDs[NUM_PLAYERS];
                int validCount = 0;

                for (int i = 0; i < size; i++) {
                    if (dataPtr[i] >= '0' && dataPtr[i] <= '9') {
                        int id = dataPtr[i] - '0';
                        if (id != myId) {
                            validIDs[validCount++] = id;
                        }
                    }
                }

                int voteTo = -1;
                bool valid = false;
                while (!valid) {
                    pc.printf("[Mafia] 죽일 ID를 입력하세요: ");
                    while (!pc.readable());
                    char ch = pc.getc();
                    pc.printf("%c", ch);

                    if (ch < '0' || ch > '9') {
                        pc.printf("\n❗ 숫자가 아닙니다.");
                        continue;
                    }

                    voteTo = ch - '0';
                    for (int i = 0; i < validCount; i++) {
                        if (validIDs[i] == voteTo) {
                            valid = true;
                            break;
                        }
                    }

                    if (!valid) {
                        pc.printf("\n❗ 유효하지 않은 ID입니다.");
                    }
                }

                char reply[4];
                sprintf(reply, "%d", voteTo);
                L3_LLI_dataReqFunc((uint8_t*)reply, strlen(reply), 1);
                pc.printf("[Mafia] %d번을 죽이기로 선택하여 Host에 전송 완료\n", voteTo);

                L3_event_clearEventFlag(L3_event_msgRcvd);
                change_state = 2;
            }

            // 4. 상태 전환
            if (change_state == 2) {
                pc.printf("💤 MAFIA 단계 종료 → DOCTOR 단계로 전환\n");
                change_state = 0;
                main_state = DOCTOR;
            }

            break;
        }

        
        case MODE_2:
            // 대기 
            pc.printf("\r\n\n모드 2 단계입니다.\n\n\n");
            
            for (int i = 0; i < 4; i++) {
            pc.printf("\r\nPlayer %d - ID: %d, Role: %s, Alive: %d\n\n", 
                    i, players[i].id, getRoleName(players[i].role), dead[i]);
            }

            main_state = DAY;
            break;


        case OVER:
        {
            static bool waitingAck = false;
            static bool waitingHostInput = false;
            static int currentSendIndex = 0;
            static char endMsg[] = "🔚 게임이 종료되었습니다. 수고하셨습니다.";

            if (myId == 1 && currentSendIndex < NUM_PLAYERS) {
                if (!waitingAck && !waitingHostInput) {
                    int destId = players[currentSendIndex].id;
                    if (players[currentSendIndex].isAlive) {
                        L3_LLI_dataReqFunc((uint8_t*)endMsg, strlen(endMsg), destId);
                        pc.printf("[HOST] %d번 플레이어에게 게임 종료 메시지 전송\n", destId);
                        waitingAck = true;
                    } else {
                        // 죽은 사람에게는 전송 생략 (혹은 보내도 됨)
                        currentSendIndex++;
                    }
                }

                if (L3_event_checkEventFlag(L3_event_msgRcvd)) {
                    uint8_t* dataPtr = L3_LLI_getMsgPtr();
                    uint8_t size = L3_LLI_getSize();

                    if (size == 3 && strncmp((char*)dataPtr, "ACK", 3) == 0 && waitingAck) {
                        pc.printf("ACK 수신됨 (플레이어 ID: %d)\n", players[currentSendIndex].id);
                        waitingAck = false;
                        waitingHostInput = true;
                        pc.printf("다음 플레이어에게 전송하려면 '1'을 입력하세요:\n");
                    }

                    L3_event_clearEventFlag(L3_event_msgRcvd);
                }

                if (waitingHostInput && pc.readable()) {
                    char c = pc.getc();
                    if (c == '1') {
                        currentSendIndex++;
                        waitingHostInput = false;
                        pc.printf("✅ 다음 플레이어로 이동합니다.\n");

                        if (currentSendIndex >= NUM_PLAYERS) {
                            pc.printf("✅ 모든 플레이어에게 게임 종료 메시지 전송 완료!\n");
                            change_state = -1;
                            main_state = L3STATE_IDLE;  // 종료 또는 재시작 대기
                        }
                    } else {
                        pc.printf("❗ '1'을 입력해야 진행됩니다.\n");
                    }
                }
            }

            if (myId != 1 && L3_event_checkEventFlag(L3_event_msgRcvd)) {
                uint8_t* dataPtr = L3_LLI_getMsgPtr();
                uint8_t size = L3_LLI_getSize();

                pc.printf("🔚 [게스트 %d] 게임 종료 메시지 수신: %.*s\n", myId, size, dataPtr);

                // ACK 응답
                const char ack[] = "ACK";
                L3_LLI_dataReqFunc((uint8_t*)ack, sizeof(ack) - 1, 1);
                pc.printf("[게스트 %d] ACK 전송 완료\n", myId);

                L3_event_clearEventFlag(L3_event_msgRcvd);
                change_state = -1;
                main_state = L3STATE_IDLE;
            }

            break;
        }


        default :
            break;
    }
}