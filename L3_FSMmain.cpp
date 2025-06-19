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


//FSM state -------------------------------------------------
#define L3STATE_IDLE                0


//state variables
static uint8_t main_state = L3STATE_IDLE; //protocol state
static uint8_t prev_state = main_state;

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

                pc.printf("\r\n나의 역할 : %.*s (length:%d)\n\n", size, dataPtr, size);

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

            players[0].isAlive = false;
            players[1].isAlive = false;
            
            // 단체 채팅 끝난 후 모드 변경
            change_state = 0;
            main_state = VOTE;
            break;
        
        }
        
        case VOTE:
        {
            static bool waitingAck = false;
            static bool waitingHostInput = false;
            static int currentSendIndex = 0;
            static int aliveIDs[NUM_PLAYERS];
            static int aliveCount = 0;
            static bool voteCompleted[NUM_PLAYERS] = {false};
            static int voteResults[NUM_PLAYERS] = {0}; // 표 수 카운트
            static char msgStr[64];
            
            // 1. 초기화: 호스트가 살아있는 목록 구성
            if (myId == 1 && change_state == 0) {
                aliveCount = 0;
                for (int i = 0; i < NUM_PLAYERS; i++) {
                    if (players[i].isAlive) {
                        aliveIDs[aliveCount++] = players[i].id;
                        players[i].Voted = 0;
                        voteCompleted[i] = false;
                        voteResults[i] = 0;
                    }
                }

                pc.printf("\r\n📢 투표를 시작합니다.\r\n");
                pc.printf("\r\n🧍 살아있는 플레이어 목록:\r\n");
                for (int i = 0; i < aliveCount; i++) {
                    pc.printf("\r\nPlayer ID: %d", aliveIDs[i]);
                }
                pc.printf("\r\n\r\n-----------------------------------------\r\n");

                currentSendIndex = 0;
                change_state = 1;
            }

            // 2. 투표 메시지 전송 단계
            if (myId == 1 && change_state == 1) {
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

                // 응답 수신 처리
                if (L3_event_checkEventFlag(L3_event_msgRcvd)) {
                    uint8_t* dataPtr = L3_LLI_getMsgPtr();
                    int fromId = L3_LLI_getSrcId();
                    int voteTo = atoi((char*)dataPtr);

                    voteResults[voteTo]++;
                    voteCompleted[currentSendIndex] = true;

                    pc.printf("\r\n🗳️ %d번 플레이어가 %d번에게 투표했습니다.", fromId, voteTo);

                    waitingAck = false;
                    waitingHostInput = true;

                    pc.printf("\r\n▶ 다음 플레이어에게 전송하려면 '1'을 입력하세요.");

                    L3_event_clearEventFlag(L3_event_msgRcvd);
                }

                // 호스트가 1을 입력해야 다음 진행
                if (waitingHostInput && pc.readable()) {
                    char c = pc.getc();
                    if (c == '1') {
                        currentSendIndex++;
                        waitingHostInput = false;
                        pc.printf("\r\n✅ 다음 플레이어로 이동합니다.");

                        if (currentSendIndex >= aliveCount) {
                            pc.printf("\r\n\r\n✅ 모든 투표 완료! 결과:\r\n");
                            for (int i = 0; i < NUM_PLAYERS; i++) {
                                if (players[i].isAlive) {
                                    pc.printf("\r\nPlayer %d: %d votes", players[i].id, voteResults[players[i].id]);
                                }
                            }
                            change_state = 2; // 다음 상태로 전환
                        }
                    } else {
                        pc.printf("\r\n❗ '1'을 입력해야 진행됩니다.");
                    }
                }
            }

            // 3. 게스트 측: 메시지 수신 → 투표 입력 → 전송
            if (myId != 1 && L3_event_checkEventFlag(L3_event_msgRcvd)) {
            uint8_t* dataPtr = L3_LLI_getMsgPtr();
            uint8_t size = L3_LLI_getSize();

            pc.printf("\r\n📨 투표 메시지 수신: %.*s", size, dataPtr);

            // 1. 메시지에서 ID 파싱
            int validIDs[NUM_PLAYERS];
            int validIDCount = 0;

            // 문자열 끝까지 탐색
            for (int i = 0; i < size; i++) {
                if (dataPtr[i] >= '0' && dataPtr[i] <= '9') {
                    int id = dataPtr[i] - '0';
                    // 자기 자신은 제외 (호스트가 보냈더라도 다시 한번 확인)
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
                pc.printf("\r\n%c", ch);

                // 숫자 여부
                if (ch < '0' || ch > '9') {
                    pc.printf("\r\n❗ 숫자가 아닙니다. 다시 입력하세요.");
                    continue;
                }

                voteTo = ch - '0';

                // 자기 자신 투표 금지 (이중 확인)
                if (voteTo == myId) {
                    pc.printf("\r\n❗ 자신에게는 투표할 수 없습니다.");
                    continue;
                }

                // 유효 ID인지 검사
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

                valid = 1;  // 유효 통과
            }

            // 투표 메시지 전송
            char ackMsg[4];
            sprintf(ackMsg, "%d", voteTo);
            L3_LLI_dataReqFunc((uint8_t*)ackMsg, strlen(ackMsg), 1);
            L3_event_clearEventFlag(L3_event_msgRcvd);
            change_state = 2;
        }




            // 4. 모두 상태 전환
            if (change_state == 2)
                main_state = DAY;

            break;
        }


        case NIGHT:
            // 대기 
            pc.printf("\r\n\n죽었음\n\n\n");
            break;

        case OVER:
        {
            change_state = -1;
            main_state = L3STATE_IDLE;
            break;
        }

        default :
            break;
    }
}