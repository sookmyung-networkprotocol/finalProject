#include "L3_FSMevent.h"
#include "L3_msg.h"
#include "L3_timer.h"
#include "L3_LLinterface.h"
#include "protocol_parameters.h"
#include "mbed.h"
#include "L2_FSMmain.h"
#include "L3_host.h"


// extern 
int change_state = 0;

// variables
int currentSendIndex = 0;    // 현재 전송 대상 플레이어 인덱스
bool waitingAck = false;     // ACK 대기 여부
int doctorTarget = -1;


//FSM state -------------------------------------------------
#define L3STATE_IDLE                0

#define NUM_PLAYERS 4
static bool dead[NUM_PLAYERS] = { false };  // 전부 살아있다고 초기화


//state variables
static uint8_t main_state = L3STATE_IDLE; //protocol state
static uint8_t prev_state = main_state;

static char myRoleName[16] = {0};
static bool idead = false;

//SDU (input)
char msgStr[20];
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
    //initialize service layer
    // pc.attach(&L3service_processInputWord, Serial::RxIrq);

    // pc.printf("Give a word to send : ");
}

void L3_FSMrun(void)
{   
    if (prev_state != main_state)
    {
        debug_if(DBGMSG_L3, "[L3] State transition from %i to %i\n", prev_state, main_state);
        prev_state = main_state;
    }

    //FSM should be implemented here! ---->>>>
    switch (main_state)
    {
        case L3STATE_IDLE: //IDLE state description
        {
            // 임시 시작점 : 바로 match 시작
            if (change_state == 0)
                main_state = MATCH;
             // 연결 기기 변경 및 FSM 초기화 : 호스트, 게스트 모두 해당
            else if (change_state == 1)
            {
                L2_FSMrun();
                main_state = MODE_1;
            }

            if (L3_event_checkEventFlag(L3_event_msgRcvd)) //if data reception event happens
            {
                //Retrieving data info.
                uint8_t* dataPtr = L3_LLI_getMsgPtr();
                uint8_t size = L3_LLI_getSize();

                debug("\r\nRECIVED MSG : %s (length:%i)\n\n", dataPtr, size);
                
                // pc.printf("Give a word to send : ");
                
                L3_event_clearEventFlag(L3_event_msgRcvd);
            }
            else if (L3_event_checkEventFlag(L3_event_dataToSend)) //if data needs to be sent (keyboard input)
            {
                //msg header setting
                strcpy((char*)sdu, (char*)originalWord);
                L3_LLI_dataReqFunc(sdu, wordLen, myDestId);

                wordLen = 0;

                // pc.printf("Give a word to send : ");

                L3_event_clearEventFlag(L3_event_dataToSend);
            }
            break;

        }
            
        case MATCH: // 임시 시작점
            main_state = MODE_1; // rcvd_matchAck_done 
            break;
        
        case MODE_1:
        {
            static bool waitingAck = false;
            static bool waitingHostInput = false;
            static int currentSendIndex = 0;
            static int myDestId = 0;
            static char msgStr[32]; 

            // 시작점 - 호스트이면 역할 배정 
            if (myId == 1 && change_state == 0) 
            { 
                createPlayers();

                for (int i = 0; i < 4; i++) {
                    pc.printf("\r\nPlayer %d - ID: %d, Role: %s\n\n", 
                            i, players[i].id, getRoleName(players[i].role));
                }

                currentSendIndex = 0;  // 초기화
                waitingAck = false;
                waitingHostInput = false;
                change_state = 1;
            } 
            
            // 중간점 - 호스트이면 역할 전송 
            if (myId == 1 && change_state == 1) {
                if (!waitingAck && !waitingHostInput && currentSendIndex < 4) {
                    myDestId = players[currentSendIndex].id;

                    strcpy(msgStr, getRoleName(players[currentSendIndex].role));
                    
                    pc.printf("\r\nSEND ROLE to ID %d : %s\n\n", myDestId, msgStr);
                    
                    L3_LLI_dataReqFunc((uint8_t*)msgStr, strlen(msgStr), myDestId);
                    waitingAck = true;
                }
                
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

                // 호스트가 '1' 입력하면 다음 전송 진행
                if (waitingHostInput) {
                    if (pc.readable()) {
                        char c = pc.getc();
                        if (c == '1') {
                            pc.printf("\r\n1 입력 확인. 다음 플레이어로 전송합니다.\n");
                            waitingHostInput = false;
                            currentSendIndex++;

                            if (currentSendIndex >= 4) {
                                pc.printf("\r\n게임이 시작되었습니다.\n\n");
                                change_state = 2;  // 모든 역할 전송 완료 후 상태 전환
                            } else {
                                // 다음 플레이어 전송 준비를 위해 다시 IDLE로 가기
                                change_state = 1;
                                main_state = L3STATE_IDLE;
                            }
                        } else {
                            pc.printf("\r\n1을 입력해야 다음 전송을 진행합니다.\n\n");
                        }
                    }
                }
            }

            // 게스트 - 메시지 수신 이벤트 처리
            if (myId != 1 && L3_event_checkEventFlag(L3_event_msgRcvd)) {
                uint8_t* dataPtr = L3_LLI_getMsgPtr();
                uint8_t size = L3_LLI_getSize();

                int len = size;
                if (len > 15) len = 15;              // 최대 크기 제한
                memcpy(myRoleName, dataPtr, len);    // 복사
                myRoleName[len] = '\0';               // null 종료

                pc.printf("\r\n나의 역할 : %.*s (length:%d)\n\n", size, myRoleName, size);

                // ACK 전송
                const char ackMsg[] = "ACK";
                L3_LLI_dataReqFunc((uint8_t*)ackMsg, sizeof(ackMsg) - 1, 1);

                L3_event_clearEventFlag(L3_event_msgRcvd);

                pc.printf("\r\n게임이 시작되었습니다.\n\n");
                change_state = 2;  // 게스트도 상태 전환
            }

            // 조건부 상태 전환만 허용
            if (change_state == 2)
                main_state = DAY;

            break;
        }

        case DAY:
        {
            pc.printf("\r\n낮이 되었습니다.\n\n");
            // 단체 채팅 구현

            
            // 단체 채팅 끝난 후 모드 변경
            change_state = 0;
            main_state = VOTE;
            break;
        
        }
        
       case VOTE:
        {
            static int voteDoneCount = 0;  // 투표 완료한 사람 수
            static int aliveCount = 0;      // 살아있는 사람 수
            static bool gameOver = false;  // 게임 종료 여부

            #define MAX_PLAYER_ID 9
            static int voteResults[MAX_PLAYER_ID + 1] = {0};
            static bool waitingAck = false;
            static bool waitingHostInput = false;
            static int currentSendIndex = 0;
            static int aliveIDs[NUM_PLAYERS];
            static bool voteCompleted[NUM_PLAYERS] = {false};

            static int maxVotedId = -1;

            static char msgStr[512]; // 메시지 버퍼 약간 키움
            
            // 1. 초기화: 호스트가 살아있는 목록 구성 및 변수 초기화
            if (myId == 1 && change_state == 0) {
                aliveCount = 0;
                voteDoneCount = 0;
                currentSendIndex = 0;
                waitingAck = false;
                waitingHostInput = false;

                for (int i = 0; i < NUM_PLAYERS; i++) {
                    if (!dead[i]) {  // 죽지 않은 플레이어만 포함
                        aliveIDs[aliveCount++] = players[i].id;
                    }
                    players[i].Voted = 0;
                    voteCompleted[i] = false;
                    voteResults[i] = 0;
                }

                pc.printf("\r\n📢 투표를 시작합니다.\r\n");
                pc.printf("\r\n🧍 살아있는 플레이어 목록:\r\n");
                for (int i = 0; i < aliveCount; i++) {
                    pc.printf("Player ID: %d ", aliveIDs[i]);
                }
                pc.printf("\r\n-----------------------------------------\r\n");

                change_state = 1;
            }


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
                static char msgStr[512]; // 결과 메시지 (한 번만 구성)
                static bool msgGenerated = false;
                static int currentSendIndex = 0;
                static bool waitingAck = false;
                static bool waitingHostInput = false;

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
                        } else if (voteResults[id] == maxVotes && id != maxVotedId) {
                            tie = true;
                        }
                    }

                    // 투표 결과 메시지 추가
                    if (!tie && maxVotedId != -1) {
                        strcat(msgStr, "\n💀 ");
                        char killBuf[64];
                        sprintf(killBuf, "%d번 플레이어가 처형되었습니다.", maxVotedId);
                        strcat(msgStr, killBuf);

                        // 실제 제거 처리
                        for (int i = 0; i < NUM_PLAYERS; i++) {
                            if (players[i].id == maxVotedId) {
                                players[i].isAlive = false;
                                break;
                            }
                        }
                    } else {
                        strcat(msgStr, "\n⚖️ 동점으로 아무도 죽지 않았습니다.");
                    }

                    // 생존 마피아/시민 수 계산
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
                        strcat(msgStr, "\n☀️ 낮으로 넘어갑니다.");
                    }

                    msgGenerated = true;
                }

                // 메시지 전송
                if (!waitingAck && !waitingHostInput && currentSendIndex < aliveCount) {
                    int destId = aliveIDs[currentSendIndex];
                    L3_LLI_dataReqFunc((uint8_t*)msgStr, strlen(msgStr), destId);
                    pc.printf("\n[Host] %d번 플레이어에게 투표 결과 전송: %s\n", destId, msgStr);
                    waitingAck = true;
                }

                // ACK 수신 처리
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

                // HOST 입력 처리
                if (waitingHostInput && pc.readable()) {
                    char c = pc.getc();
                    if (c == '1') {
                        currentSendIndex++;
                        waitingHostInput = false;
                        pc.printf("✅ 다음 플레이어로 이동합니다.\n");

                        if (currentSendIndex >= aliveCount) {
                            pc.printf("✅ 모든 투표 결과 전송 완료!\n");
                            msgGenerated = false;
                            currentSendIndex = 0;
                            change_state = 3;
                        }
                    } else {
                        pc.printf("❗ '1'을 입력해야 진행됩니다.\n");
                    }
                }
            }


            // 5. 게스트 - 투표 결과 수신 및 ACK 전송
            if (myId != 1 && change_state == 2 && L3_event_checkEventFlag(L3_event_msgRcvd))  {
                uint8_t* dataPtr = L3_LLI_getMsgPtr();
                uint8_t size = L3_LLI_getSize();

                pc.printf("\r\n[게스트 %d] 투표 결과 수신:\n%.*s\n", myId, size, dataPtr);

                int killedId = -1;
                // "💀 <id>번 플레이어가 처형되었습니다." 부분 찾기
                char* killPos = strstr((char*)dataPtr, "💀 ");
                if (killPos != NULL) {
                    // 처형된 플레이어 ID 파싱
                    int parsedId = -1;
                    sscanf(killPos, "💀 %d번 플레이어가 처형되었습니다.", &parsedId);
                    killedId = parsedId;
                    pc.printf("💀 %d번 플레이어가 처형됨\n", killedId);
                } else {
                    pc.printf("⚖️ 동점으로 처형된 플레이어 없음\n");
                }

                // 자기 자신이 죽었으면 상태 변경
                if (killedId == myId) {
                    pc.printf("❗ 당신은 처형되었습니다.\n");
                    idead = true;
                }

                // 게임 종료 메시지 확인
                if (strstr((char*)dataPtr, "시민 승리") != NULL) {
                    pc.printf("🎉 시민 승리! 게임 종료 처리 필요\n");
                    gameOver = true;
                } else if (strstr((char*)dataPtr, "마피아 승리") != NULL) {
                    pc.printf("💀 마피아 승리! 게임 종료 처리 필요\n");
                    gameOver = true;
                }

                // ACK 전송
                const char ackMsg[] = "ACK";
                L3_LLI_dataReqFunc((uint8_t*)ackMsg, sizeof(ackMsg) - 1, 1);
                pc.printf("[게스트 %d] ACK 전송 완료\n", myId);

                L3_event_clearEventFlag(L3_event_msgRcvd);

                change_state = 3;  // 투표 종료 상태로 변경
            }


            // 6. 모두 상태 전환: 투표 종료 시 낮(주간) 상태로 전환
           if (change_state == 3) {
            // 💀 밤에 마피아가 선택한 타겟 적용
            int mafiaTarget = -1;
            for (int i = 0; i < NUM_PLAYERS; i++) {
                if (players[i].role == ROLE_MAFIA && players[i].isAlive) {
                    mafiaTarget = players[i].sentVoteId;
                    break;
                }
            }

            // 🩺 의사 효과 반영: 마피아 타겟이 doctorTarget이면 무효, 아니면 죽음
            if (mafiaTarget != -1) {
                for (int i = 0; i < NUM_PLAYERS; i++) {
                    if (players[i].id == mafiaTarget &&
                        players[i].isAlive &&
                        players[i].id != doctorTarget)
                    {
                        players[i].isAlive = false;
                        pc.printf("💀 밤에 %d번 플레이어가 죽었습니다.\n", players[i].id);
                    }
                }
            }

            // 🔁 idead 동기화 (자기 자신이 죽었을 경우)
            for (int i = 0; i < NUM_PLAYERS; i++) {
                if (players[i].id == myId && !players[i].isAlive) {
                    idead = true;
                }
            }

            // 🔄 sentVoteId, doctorTarget 초기화
            for (int i = 0; i < NUM_PLAYERS; i++) {
                players[i].sentVoteId = -1;
            }
            doctorTarget = -1;

            // 🧹 투표 관련 변수 초기화
            voteDoneCount = 0;
            for (int i = 0; i < NUM_PLAYERS; i++) {
                voteResults[i] = 0;
                voteCompleted[i] = false;
                players[i].Voted = 0;
            }

            // 🔄 상태 전환 준비
            change_state = 0;

            // 🏁 게임 종료 여부 체크
            if (gameOver) {
                main_state = OVER;
            } else if (myId == 1) {
                main_state = MAFIA;
            } else {
                // 디버깅용 출력
                pc.printf("내 번호는 %d입니다.\n", myId);
                pc.printf("내 역할은 %s입니다.\n", myRoleName);
                pc.printf("내 생존 상태: %s\n", idead ? "죽음" : "살아있음");

                // 살아있을 경우 역할별 분기
                if (!idead) {
                    if (strcmp(myRoleName, "Mafia") == 0)
                        main_state = MAFIA;
                    else if (strcmp(myRoleName, "Police") == 0)
                        main_state = POLICE;
                    else if (strcmp(myRoleName, "Doctor") == 0)
                        main_state = DOCTOR;
                    else
                        main_state = NIGHT;
                } else {
                    main_state = NIGHT;
                }
            }
        }




            break;
        }


        case NIGHT:
            // 대기 
            pc.printf("\r\n\n밤이 되었습니다.\n\n\n");
            main_state = DAY;
            break;

        

        case POLICE:
        {
            static bool sentToPolice = false;
            static int policeId = -1;

            // 생존자 상태 업데이트 (매번 체크)
            for (int i = 0; i < NUM_PLAYERS; i++) {
                if (players[i].id == myId && !players[i].isAlive) {
                    idead = true;
                }
            }

            // Host
            if (myId == 1) {
                if (change_state == 0) {
                    // 경찰 찾기
                    policeId = -1;
                    for (int i = 0; i < NUM_PLAYERS; i++) {
                        if (players[i].role == ROLE_POLICE && players[i].isAlive) {
                            policeId = players[i].id;
                            break;
                        }
                    }

                    if (policeId == -1) {
                        pc.printf("👮‍♂️ [HOST] 살아있는 경찰 없음. 3초 후 DAY로 전환\n");
                        change_state = 1; // 대기 상태로
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
                        change_state = 2; // 경찰 응답 대기
                    }
                }
                else if (change_state == 2 && L3_event_checkEventFlag(L3_event_msgRcvd)) {
                    // 경찰 응답 처리
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

                    L3_LLI_dataReqFunc((uint8_t*)roleStr, strlen(roleStr), policeId);
                    pc.printf("[HOST] %d번의 정체 '%s'를 %d번 경찰에게 전송 완료\n", targetId, roleStr, policeId);

                    L3_event_clearEventFlag(L3_event_msgRcvd);
                    change_state = 1; // 대기 상태로
                }
                
                // Host 대기 후 DAY 전환 (경찰 없거나 업무 완료 후)
                if (change_state == 1) {
                    static int hostWaitCounter = 0;
                    hostWaitCounter++;
                    
                    if (hostWaitCounter > 3000) { // 3초 대기
                        pc.printf("🌤️ [HOST] POLICE 단계 종료 → DAY로 전환\n");
                        main_state = DAY;
                        change_state = 0;
                        hostWaitCounter = 0;
                        sentToPolice = false;
                        policeId = -1;
                    }
                }
            }
            // 경찰
            else if (strcmp(myRoleName, "Police") == 0 && !idead) {
                if (change_state == 0 && L3_event_checkEventFlag(L3_event_msgRcvd)) {
                    uint8_t* dataPtr = L3_LLI_getMsgPtr();
                    uint8_t size = L3_LLI_getSize();
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
                    change_state = 1; // 정체 응답 대기
                }
                else if (change_state == 1 && L3_event_checkEventFlag(L3_event_msgRcvd)) {
                    uint8_t* dataPtr = L3_LLI_getMsgPtr();
                    uint8_t size = L3_LLI_getSize();
                    pc.printf("[Police] 수신된 정체: %.*s\n", size, dataPtr);

                    L3_event_clearEventFlag(L3_event_msgRcvd);
                    change_state = 2; // 대기 상태로
                }
                
                // 경찰 대기 후 DAY 전환
                if (change_state == 2) {
                    static int policeWaitCounter = 0;
                    policeWaitCounter++;
                    
                    if (policeWaitCounter > 2000) { // 2초 대기
                        pc.printf("🌤️ [경찰] POLICE 단계 종료 → DAY로 전환\n");
                        main_state = DAY;
                        change_state = 0;
                        policeWaitCounter = 0;
                    }
                }
            }
            // 마피아
            else if (strcmp(myRoleName, "Mafia") == 0) {
                static int mafiaWaitCounter = 0;
                mafiaWaitCounter++;
                
                if (mafiaWaitCounter > 5000) { // 5초 대기
                    pc.printf("🌤️ [마피아] POLICE 단계 대기 완료 → DAY로 전환\n");
                    main_state = DAY;
                    change_state = 0;
                    mafiaWaitCounter = 0;
                }
            }
            // 의사
            else if (strcmp(myRoleName, "Doctor") == 0) {
                static int doctorWaitCounter = 0;
                doctorWaitCounter++;
                
                if (doctorWaitCounter > 5000) { // 5초 대기
                    pc.printf("🌤️ [의사] POLICE 단계 대기 완료 → DAY로 전환\n");
                    main_state = DAY;
                    change_state = 0;
                    doctorWaitCounter = 0;
                }
            }
            // 시민
            else if (strcmp(myRoleName, "Citizen") == 0) {
                static int citizenWaitCounter = 0;
                citizenWaitCounter++;
                
                if (citizenWaitCounter > 5000) { // 5초 대기
                    pc.printf("🌤️ [시민] POLICE 단계 대기 완료 → DAY로 전환\n");
                    main_state = DAY;
                    change_state = 0;
                    citizenWaitCounter = 0;
                }
            }
            // 죽은 플레이어
            else if (idead) {
                static int deadWaitCounter = 0;
                deadWaitCounter++;
                
                if (deadWaitCounter > 3000) { // 3초 대기
                    pc.printf("🌤️ [죽은 플레이어] POLICE 단계 대기 완료 → DAY로 전환\n");
                    main_state = DAY;
                    change_state = 0;
                    deadWaitCounter = 0;
                }
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