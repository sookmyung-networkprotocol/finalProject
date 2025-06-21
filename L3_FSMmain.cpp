#include "L3_shared.h"
#include "L3_state_idle.h"
#include "L3_state_day.h"
#include "L3_state_vote.h"
#include "L3_state_night.h"
#include "L3_state_over.h"

// FSM 상태 정의
typedef enum {
    L3STATE_IDLE = 0,
    MATCH,
    MODE_1,
    DAY,
    VOTE,
    NIGHT,
    MODE_2,
    MAFIA,
    DOCTOR,
    POLICE,
    OVER,
    TYPING
} L3State;

// 상태 변수
static L3State main_state = L3STATE_IDLE;
static L3State prev_state = L3STATE_IDLE;

// 시리얼 인터페이스
static Serial pc(USBTX, USBRX);

// 키보드 입력 처리 함수 (입력 완료 시 sdu에 복사)
static void L3service_processInputWord() {
    char c = pc.getc();

    if (!L3_event_checkEventFlag(L3_event_dataToSend)) {
        if (c == '\n' || c == '\r') {
            originalWord[wordLen++] = '\0';
            memcpy(sdu, originalWord, wordLen);
            L3_event_setEventFlag(L3_event_dataToSend);
            wordLen = 0;
        } else if (wordLen < sizeof(originalWord) - 1) {
            originalWord[wordLen++] = c;
        }
    }
}

// FSM 초기화 함수
void L3_initFSM(uint8_t Id, uint8_t destId) {
    myId = Id;
    myDestId = destId;

    pc.attach(&L3service_processInputWord, Serial::RxIrq);
    printf("[L3] FSM initialized. MyID: %d, DestID: %d\n", myId, myDestId);
}

// FSM 실행 루프
void L3_FSMrun(void) {
    if (prev_state != main_state) {
        printf("[L3] State transition from %d to %d\n", prev_state, main_state);
        prev_state = main_state;
    }

    switch (main_state) {
        case L3STATE_IDLE:
            L3_handleIdle();
            break;
        case MATCH:
            L3_handleMatch();
            break;
        case MODE_1:
            L3_handleMode1();
            break;
        case DAY:
            L3_handleDay();
            break;
        case VOTE:
            L3_handleVote();
            break;
        case MAFIA:
            L3_handleMafia();
            break;
        case DOCTOR:
            L3_handleDoctor();
            break;
        case POLICE:
            L3_handlePolice();
            break;
        case NIGHT:
            L3_handleNight();
            break;
        case MODE_2:
            L3_handleMode2();
            break;
        case OVER:
            L3_handleOver();
            break;
        default:
            break;
    }
}
