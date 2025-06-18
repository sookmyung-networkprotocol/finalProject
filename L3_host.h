#include <stdint.h>
#include <stdbool.h>

//FSM state -------------------------------------------------
#define MATCH                1
#define MODE_1               2
#define DAY                  3
#define VOTE                 4
#define MAFIA                5
#define DOCTOR               6
#define POLICE               7
#define MODE_2               8
#define OVER                 9

extern int change_state;

// 역할 정의
typedef enum {
    ROLE_CITIZEN,
    ROLE_MAFIA,
    ROLE_POLICE,
    ROLE_DOCTOR
} Role;

// 플레이어 객체 정의
typedef struct {
    uint8_t id;            // 게스트 ID
    Role role;             // 역할
    bool isAlive;          // 생존 여부
    int8_t sentVoteId;     // 투표한 대상 ID (-1: 투표 안함)
} Player;

// extern
extern Player players[4];


// 초기화 함수
void initPlayer(Player* p, uint8_t id, Role role);

// 역할 배정 함수 
void createPlayers();

// 역할 문자열 반환 함수
const char* getRoleName(Role r);