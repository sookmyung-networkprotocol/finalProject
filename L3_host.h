#include <stdint.h>
#include <stdbool.h>

//FSM state -------------------------------------------------
#define MATCH                1
#define MODE_1               2
#define DAY                  3
#define VOTE                 4
#define NIGHT                5
#define MODE_2               6
#define MAFIA                7
#define DOCTOR               8
#define POLICE               9
#define TYPING               11

#define OVER                 12
//FSM state -------------------------------------------------


extern int change_state;
#define NUM_PLAYERS 4
#define NUM_ROLES 4

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
    int8_t Voted;          // 받은 표 수
    int8_t sentVoteId; 
} Player;

// extern
extern Player players[4];

// 플레이어 ID 목록
extern Player players[NUM_PLAYERS]; 

// 초기화 함수
void initPlayer(Player* p, uint8_t id, Role role);

// 역할 배정 함수 
void createPlayers();

// 역할 문자열 반환 함수
const char* getRoleName(Role r);