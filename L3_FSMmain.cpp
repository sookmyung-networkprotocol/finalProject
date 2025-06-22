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

        // DAY 상태에서 그룹 채팅 시뮬레이션 코드
// L3_FSMmain.cpp의 DAY case 안에 추가할 코드

        // DAY 상태에서 그룹 채팅 시뮬레이션 코드 (두 번째 날)
// L3_FSMmain.cpp의 DAY case 안에 추가할 코드

        case DAY:
        {
            static bool chatStarted = false;
            static int chatStep = 0;
            static Timer chatTimer;
            static bool waitingForVote = false;
            static bool chatCompleted = false;
            
            // 두 번째 시나리오: 첫 번째 밤 이후 두 번째 낮
            // 의사가 시민(3번)을 구해서 4명 모두 생존
            // Player 2(Police), Player 3(Citizen), Player 7(Mafia), Player 8(Doctor)
            // 결과: 시민(3번)이 마피아로 의심받아 처형됨
            
            if (!chatStarted) {
                pc.printf("\r\n🌅 두 번째 날이 밝았습니다!\r\n");
                pc.printf("🩺 의사가 누군가를 구해서 아무도 죽지 않았습니다.\r\n");
                pc.printf("💬 그룹 채팅을 시작합니다.\r\n");
                if (myId == 1) {
                    pc.printf("🎮 호스트는 'v'를 눌러 투표 단계로 넘어갈 수 있습니다.\r\n");
                }
                pc.printf("💬 > ");
                
                chatTimer.start();
                chatStarted = true;
                chatStep = 0;
            }
            
            // 시뮬레이션된 그룹 채팅 메시지들 (시간차를 두고 출력)
            if (!chatCompleted) {
                float elapsed = chatTimer.read();
                
                switch(chatStep) {
                    case 0:
                        if (elapsed > 4.0f) {
                            pc.printf("\r\n[Player 8] 와! 의사가 정말 잘했네요. 모두 살아있어요!\r\n");
                            pc.printf("💬 > ");
                            chatStep++;
                            chatTimer.reset();
                        }
                        break;
                        
                    case 1:
                        if (elapsed > 6.5f) {
                            pc.printf("\r\n[Player 2] 네, 다행이에요. 그런데 마피아가 누구를 공격했을까요?\r\n");
                            pc.printf("💬 > ");
                            chatStep++;
                            chatTimer.reset();
                        }
                        break;
                        
                    case 2:
                        if (elapsed > 5.5f) {
                            pc.printf("\r\n[Player 7] 저도 궁금해요. 의사가 정확히 맞춘 것 같네요.\r\n");
                            pc.printf("💬 > ");
                            chatStep++;
                            chatTimer.reset();
                        }
                        break;
                        
                    case 3:
                        if (elapsed > 7.0f) {
                            pc.printf("\r\n[Player 3] 어제 저를 의심한 사람들이 있었는데... 마피아가 저를 노린 게 아닐까요?\r\n");
                            pc.printf("💬 > ");
                            chatStep++;
                            chatTimer.reset();
                        }
                        break;
                        
                    case 4:
                        if (elapsed > 6.0f) {
                            pc.printf("\r\n[Player 7] 오, 그럴 수도 있겠네요. 3번이 시민이라면 마피아가 위협적으로 느꼈을 거예요.\r\n");
                            pc.printf("💬 > ");
                            chatStep++;
                            chatTimer.reset();
                        }
                        break;
                        
                    case 5:
                        if (elapsed > 8.0f) {
                            pc.printf("\r\n[Player 2] 잠깐... 그런데 3번이 자기가 타겟이었다고 추측하는 게 좀 이상하지 않나요?\r\n");
                            pc.printf("💬 > ");
                            chatStep++;
                            chatTimer.reset();
                        }
                        break;
                        
                    case 6:
                        if (elapsed > 5.0f) {
                            pc.printf("\r\n[Player 3] 어? 왜요? 어제 저를 의심했잖아요!\r\n");
                            pc.printf("💬 > ");
                            chatStep++;
                            chatTimer.reset();
                        }
                        break;
                        
                    case 7:
                        if (elapsed > 7.5f) {
                            pc.printf("\r\n[Player 8] 음... 2번 말이 일리가 있어요. 일반적으로 자기가 타겟이었다고 말하지 않죠.\r\n");
                            pc.printf("💬 > ");
                            chatStep++;
                            chatTimer.reset();
                        }
                        break;
                        
                    case 8:
                        if (elapsed > 6.5f) {
                            pc.printf("\r\n[Player 7] 맞아요! 마피아가 오히려 자기는 안전하다는 걸 어필하려는 거 아닌가요?\r\n");
                            pc.printf("💬 > ");
                            chatStep++;
                            chatTimer.reset();
                        }
                        break;
                        
                    case 9:
                        if (elapsed > 4.5f) {
                            pc.printf("\r\n[Player 3] 그게 아니에요! 저는 정말 시민이라고요!\r\n");
                            pc.printf("💬 > ");
                            chatStep++;
                            chatTimer.reset();
                        }
                        break;
                        
                    case 10:
                        if (elapsed > 7.0f) {
                            pc.printf("\r\n[Player 2] 어제도 3번이 너무 적극적으로 7번을 의심했는데... 혹시 시선 돌리기였나요?\r\n");
                            pc.printf("💬 > ");
                            chatStep++;
                            chatTimer.reset();
                        }
                        break;
                        
                    case 11:
                        if (elapsed > 5.5f) {
                            pc.printf("\r\n[Player 8] 생각해보니 3번이 어제 가장 공격적이었어요.\r\n");
                            pc.printf("💬 > ");
                            chatStep++;
                            chatTimer.reset();
                        }
                        break;
                        
                    case 12:
                        if (elapsed > 6.0f) {
                            pc.printf("\r\n[Player 7] 그러게요! 저는 그냥 방어만 했는데 3번이 계속 저를 몰아세웠어요.\r\n");
                            pc.printf("💬 > ");
                            chatStep++;
                            chatTimer.reset();
                        }
                        break;
                        
                    case 13:
                        if (elapsed > 8.5f) {
                            pc.printf("\r\n[Player 3] 아니에요! 저는 진짜 마피아를 찾으려고 했던 거예요! 7번이 수상했다니까요!\r\n");
                            pc.printf("💬 > ");
                            chatStep++;
                            chatTimer.reset();
                        }
                        break;
                        
                    case 14:
                        if (elapsed > 6.5f) {
                            pc.printf("\r\n[Player 2] 하지만 지금 3번의 행동 패턴을 보면 마피아 같아요.\r\n");
                            pc.printf("💬 > ");
                            chatStep++;
                            chatTimer.reset();
                        }
                        break;
                        
                    case 15:
                        if (elapsed > 5.0f) {
                            pc.printf("\r\n[Player 8] 저도 3번에게 투표하는 게 좋을 것 같아요.\r\n");
                            pc.printf("💬 > ");
                            chatStep++;
                            chatTimer.reset();
                        }
                        break;
                        
                    case 16:
                        if (elapsed > 7.0f) {
                            pc.printf("\r\n[Player 7] 네, 저도 3번이 마피아라고 생각해요. 어제부터 계속 의심스러웠어요.\r\n");
                            pc.printf("💬 > ");
                            chatStep++;
                            chatTimer.reset();
                        }
                        break;
                        
                    case 17:
                        if (elapsed > 4.0f) {
                            pc.printf("\r\n[Player 3] 이건 말이 안 돼요... 여러분 속지 마세요!\r\n");
                            pc.printf("💬 > ");
                            chatStep++;
                            chatTimer.reset();
                        }
                        break;
                        
                    case 18:
                        if (elapsed > 6.0f) {
                            pc.printf("\r\n[Player 2] 죄송하지만 3번, 증거가 너무 많아요.\r\n");
                            pc.printf("💬 > ");
                            chatStep++;
                            chatTimer.reset();
                        }
                        break;
                        
                    case 19:
                        if (elapsed > 5.5f) {
                            pc.printf("\r\n[Player 8] 투표로 결정하죠. 3번에게 투표할게요.\r\n");
                            pc.printf("💬 > ");
                            chatStep++;
                            chatTimer.reset();
                        }
                        break;
                        
                    case 20:
                        if (elapsed > 3.0f) {
                            pc.printf("\r\n📢 그룹 채팅이 종료되었습니다.\r\n");
                            pc.printf("🗳️ 투표 결과: 3번이 처형될 예정입니다.\r\n\r\n");
                            chatCompleted = true;
                            waitingForVote = true;
                        }
                        break;
                }
            }
            
            // 호스트가 'v' 키를 누르면 바로 투표로 넘어가기
            if (myId == 1 && !chatCompleted && pc.readable()) {
                char c = pc.getc();
                if (c == 'v' || c == 'V') {
                    pc.printf("\r\n🗳️ 투표 단계로 이동합니다.\r\n");
                    chatCompleted = true;
                    waitingForVote = true;
                }
            }
            
            // 채팅이 완료되면 투표 상태로 전환
            if (waitingForVote && chatCompleted) {
                change_state = 0;
                main_state = VOTE;
                chatStarted = false;  // 다음을 위해 리셋
                chatStep = 0;
                waitingForVote = false;
                chatCompleted = false;
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
                change_state = 0;
            } else {
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
            
            // 마피아 업무 완료 - DOCTOR 단계로 전환
            pc.printf("💤 MAFIA 단계 종료 → DOCTOR 단계로 전환\n");
            main_state = DOCTOR;
            change_state = 0;
            
            // 초기화
            waitingAck = false;
            mafiaMessageSent = false;
            mafiaId = -1;
            aliveCount = 0;
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
            change_state = 1; // 대기 상태
        }

        // 4. Guest(마피아): 대기 후 DOCTOR로 전환
        if (myId != 1 && strcmp(myRoleName, "Mafia") == 0 && change_state == 1) {
            static int mafiaWaitCounter = 0;
            mafiaWaitCounter++;
            
            if (mafiaWaitCounter > 2000) { // 2초 대기
                pc.printf("💤 [마피아] MAFIA 단계 종료 → DOCTOR 단계로 전환\n");
                main_state = DOCTOR;
                change_state = 0;
                mafiaWaitCounter = 0;
            }
        }

        // 5. 다른 Guest들: 일정 시간 후 DOCTOR로 전환
        if (myId != 1 && strcmp(myRoleName, "Mafia") != 0) {
            static int otherWaitCounter = 0;
            otherWaitCounter++;
            
            if (otherWaitCounter > 3000) { // 3초 대기
                pc.printf("💤 [%s] MAFIA 단계 대기 완료 → DOCTOR 단계로 전환\n", myRoleName);
                main_state = DOCTOR;
                change_state = 0;
                otherWaitCounter = 0;
            }
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