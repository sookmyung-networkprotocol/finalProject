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

                debug("\n ------------------\nRECIVED MSG : %s (length:%i)\n --------------------------\n", dataPtr, size);
                
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
            static char msgStr[32];  // 충분한 크기 확보

            // 시작점 - 호스트이면 역할 배정 
            if (myId == 1 && change_state == 0) 
            { 
                createPlayers();

                for (int i = 0; i < 4; i++) {
                    pc.printf("Player %d - ID: %d, Role: %s\n", 
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
                    
                    pc.printf("SEND ROLE to ID %d : %s\n", myDestId, msgStr);
                    
                    L3_LLI_dataReqFunc((uint8_t*)msgStr, strlen(msgStr), myDestId);
                    waitingAck = true;
                }
                
                if (L3_event_checkEventFlag(L3_event_msgRcvd)) {
                    uint8_t* dataPtr = L3_LLI_getMsgPtr();
                    uint8_t size = L3_LLI_getSize();
                    
                    if (size == 3 && strncmp((char*)dataPtr, "ACK", 3) == 0 && waitingAck) {
                        pc.printf("ACK received from player ID %d\n", myDestId);
                        waitingAck = false;
                        waitingHostInput = true;
                        pc.printf("다음 플레이어에게 전송할까요? (1 입력)\n");
                    }

                    L3_event_clearEventFlag(L3_event_msgRcvd);
                }

                // 호스트가 '1' 입력하면 다음 전송 진행
                if (waitingHostInput) {
                    if (pc.readable()) {
                        char c = pc.getc();
                        if (c == '1') {
                            pc.printf("1 입력 확인. 다음 플레이어로 전송합니다.\n");
                            waitingHostInput = false;
                            currentSendIndex++;

                            if (currentSendIndex >= 4) {
                                pc.printf("------------------------------END------------------------------\n");
                                change_state = 2;  // 모든 역할 전송 완료 후 상태 전환
                            } else {
                                // 다음 플레이어 전송 준비를 위해 다시 IDLE로 가기
                                change_state = 1;
                                main_state = L3STATE_IDLE;
                            }
                        } else {
                            pc.printf("1을 입력해야 다음 전송을 진행합니다.\n");
                        }
                    }
                }
            }

            // 게스트 - 메시지 수신 이벤트 처리
            if (myId != 1 && L3_event_checkEventFlag(L3_event_msgRcvd)) {
                uint8_t* dataPtr = L3_LLI_getMsgPtr();
                uint8_t size = L3_LLI_getSize();

                pc.printf("\nRECEIVED ROLE MSG: %.*s (length:%d)\n", size, dataPtr, size);

                // ACK 전송
                const char ackMsg[] = "ACK";
                L3_LLI_dataReqFunc((uint8_t*)ackMsg, sizeof(ackMsg) - 1, 1);

                L3_event_clearEventFlag(L3_event_msgRcvd);

                pc.printf("------------------------------END------------------------------\n");
                change_state = 2;  // 게스트도 상태 전환
            }

            // 조건부 상태 전환만 허용
            if (change_state == 2)
                main_state = L3STATE_IDLE;

            break;
        }

        case DAY:
        {
            pc.printf("-----------------------------------------\n\n낮이 되었습니다.\n\n----------------------------------------");
            // 단체 채팅 구현 

            // 단체 채팅 끝난 후 모드 변경
            main_state = VOTE;
            break;
        
        }
        
        case VOTE:
        {
            change_state = 1;

            if (myId == 1) {  // 호스트
                if (change_state == 0) {
                    currentSendIndex = 0;
                    waitingAck = false;
                    change_state = 1;

                    // 투표 수 초기화
                    for (int i = 0; i < NUM_PLAYERS; i++) {
                        players[i].Voted = 0;
                    }
                }

                if (change_state == 1 && !waitingAck && currentSendIndex < 4) {
                    myDestId = players[currentSendIndex].id;

                    // 살아있는 플레이어 ID만 문자열로 만들기
                    char voteListStr[64] = {0};
                    for (int i = 0; i < NUM_PLAYERS; i++) {
                        if (players[i].isAlive) {
                            char temp[8];
                            sprintf(temp, "%d ", players[i].id);
                            strcat(voteListStr, temp);
                        }
                    }

                    sprintf(msgStr, "투표하세요. 투표 가능한 사람은 %s입니다. : ", voteListStr);

                    pc.printf("SEND VOTE to ID %d : %s\n", myDestId, msgStr);

                    L3_LLI_dataReqFunc((uint8_t*)msgStr, strlen(msgStr), myDestId);
                    waitingAck = true;
                }

                if (waitingAck && L3_event_checkEventFlag(L3_event_msgRcvd)) {
                    uint8_t* dataPtr = L3_LLI_getMsgPtr();
                    uint8_t size = L3_LLI_getSize();

                    if (size > 0) {
                        pc.printf("Received from ID %d: %.*s\n", myDestId, size, dataPtr);

                        if (size == 3 && strncmp((char*)dataPtr, "ACK", 3) == 0) {
                            pc.printf("ACK received from player ID %d\n", myDestId);
                        } else {
                            int votedId = atoi((char*)dataPtr);
                            pc.printf("Player %d voted for %d\n", myDestId, votedId);

                            // 투표 결과 집계: votedId에 해당하는 플레이어 Voted 증가
                            for (int i = 0; i < NUM_PLAYERS; i++) {
                                if (players[i].id == votedId) {
                                    players[i].Voted++;
                                    break;
                                }
                            }
                        }

                        waitingAck = false;
                        currentSendIndex++;

                        if (currentSendIndex >= 4) {
                            change_state = 2;

                            // 투표 결과 출력
                            pc.printf("\n--- 투표 결과 ---\n");
                            for (int i = 0; i < NUM_PLAYERS; i++) {
                                pc.printf("Player ID %d : %d 표\n", players[i].id, players[i].Voted);
                            }
                        } else {
                            main_state = L3STATE_IDLE;
                        }
                    }
                    L3_event_clearEventFlag(L3_event_msgRcvd);
                }
            }
            else {  // 게스트
                if (L3_event_checkEventFlag(L3_event_msgRcvd)) {
                    uint8_t* dataPtr = L3_LLI_getMsgPtr();
                    uint8_t size = L3_LLI_getSize();

                    pc.printf("\nRECEIVED VOTE REQUEST: %.*s (length:%d)\n", size, dataPtr, size);

                    int voteNum = -1;
                    pc.printf("투표 번호를 입력하세요: ");
                    pc.scanf("%d", &voteNum);

                    char voteMsg[16];
                    sprintf(voteMsg, "%d", voteNum);

                    L3_LLI_dataReqFunc((uint8_t*)voteMsg, strlen(voteMsg), 1);  // 1 = 호스트 ID

                    L3_event_clearEventFlag(L3_event_msgRcvd);

                    change_state = 2;
                }
            }

            if (change_state == 2) {
                main_state = DAY;
            }
            break;
        }

        case NIGHT:
            // 대기 
            pc.printf("죽었음");

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