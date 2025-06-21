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

// 메시지 타입 정의 -------------------------------------------------
#define MSG_TYPE_MATCH_REQ    20
#define MSG_TYPE_MATCH_ACK    21  
#define MSG_TYPE_MATCH_CNF    22
#define MSG_TYPE_MODE_DAY     23
#define MSG_TYPE_MODE_VOTE    24
#define MSG_TYPE_MODE_NIGHT   25
#define MSG_TYPE_MODE_OVER    26
#define MSG_TYPE_VOTE_ID      27
#define MSG_TYPE_KILL         28
#define MSG_TYPE_GC           29  // Group Chat
#define MSG_TYPE_ROLE_ASSIGN  30
#define MSG_TYPE_PHASE_END    31  // 단계 종료 신호
#define MSG_TYPE_GAME_STATE   32  // 게임 상태 동기화
// 메시지 타입 정의 끝 -------------------------------------------------

#define NUM_PLAYERS 4
#define NUM_ROLES 4

// 게임 상태 변수들 (NUM_PLAYERS 정의 후에 위치) -------------------------------------------------
extern int change_state;
extern bool isHost;
extern bool isInTeam;
extern int playerCnt;
extern int rcvdCnfCnt;
extern int doctorTarget;
extern bool dead[NUM_PLAYERS];  // 이제 NUM_PLAYERS가 정의된 후라서 문제없음
// 게임 상태 변수들 끝 -------------------------------------------------

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
    int8_t sentVoteId;     // 밤/투표에서 선택한 ID
} Player;

// 전역 변수들
extern Player players[NUM_PLAYERS]; 

// 플레이어 관리 함수들
void initPlayer(Player* p, uint8_t id, Role role);
void createPlayers();
const char* getRoleName(Role r);

// 게임 로직 함수들
bool is_game_over();
bool all_sent_vote_id();
int get_most_voted_player();
void kill_player(int playerId);
void reset_vote_ids();
void reset_game_variables();

// 매치메이킹 함수들
void init_match_variables();
bool is_match_complete();
void add_player_to_match(uint8_t playerId);

// 게임 상태 동기화 함수들
void broadcast_game_state(uint8_t state);
void send_phase_end_signal(uint8_t phase);