#include <time.h>
#include <cstdlib>   
#include <ctime> 
#include "L3_host.h"

Player players[NUM_PLAYERS];
static uint8_t playerIds[NUM_PLAYERS] = {2, 3, 7, 8};

// 게임 상태 변수들
static int playerCnt = 0;
static int rcvdCnfCnt = 0;
static bool isInTeam = false;
static bool isHost = false;

// 게임 로직 함수들 추가
bool is_game_over() {
    int aliveMafia = 0;
    int aliveCitizen = 0;
    
    for (int i = 0; i < NUM_PLAYERS; i++) {
        if (players[i].isAlive) {
            if (players[i].role == ROLE_MAFIA) {
                aliveMafia++;
            } else {
                aliveCitizen++;
            }
        }
    }
    
    // 마피아가 0명이거나, 마피아 >= 시민인 경우 게임 종료
    return (aliveMafia == 0) || (aliveMafia >= aliveCitizen);
}

bool all_sent_vote_id() {
    for (int i = 0; i < NUM_PLAYERS; i++) {
        if (players[i].isAlive && players[i].sentVoteId == -1) {
            return false;
        }
    }
    return true;
}

int get_most_voted_player() {
    int voteCount[NUM_PLAYERS] = {0};
    int maxVotes = 0;
    int mostVotedPlayer = -1;
    bool tie = false;
    
    // 투표 집계
    for (int i = 0; i < NUM_PLAYERS; i++) {
        if (players[i].isAlive && players[i].sentVoteId != -1) {
            for (int j = 0; j < NUM_PLAYERS; j++) {
                if (players[j].id == players[i].sentVoteId) {
                    voteCount[j]++;
                    break;
                }
            }
        }
    }
    
    // 최다 득표자 찾기
    for (int i = 0; i < NUM_PLAYERS; i++) {
        if (voteCount[i] > maxVotes) {
            maxVotes = voteCount[i];
            mostVotedPlayer = players[i].id;
            tie = false;
        } else if (voteCount[i] == maxVotes && voteCount[i] > 0) {
            tie = true;
        }
    }
    
    return tie ? -1 : mostVotedPlayer;
}

void kill_player(int playerId) {
    for (int i = 0; i < NUM_PLAYERS; i++) {
        if (players[i].id == playerId) {
            players[i].isAlive = false;
            break;
        }
    }
}

void reset_vote_ids() {
    for (int i = 0; i < NUM_PLAYERS; i++) {
        players[i].sentVoteId = -1;
    }
}

// 기존 함수들...
void initPlayer(Player* p, uint8_t id, Role role) {
    p->id = id;
    p->role = role;
    p->isAlive = true;
    p->Voted = 0;
    p->sentVoteId = -1; 
}

void shuffleRoles(Role *roles, int n) {
    for (int i = n - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        Role temp = roles[i];
        roles[i] = roles[j];
        roles[j] = temp;
    }
}

void createPlayers() {
    srand(time(NULL));

    Role availableRoles[NUM_ROLES];
    for (int i = 0; i < NUM_ROLES; i++) {
        availableRoles[i] = (Role)i;
    }

    shuffleRoles(availableRoles, NUM_ROLES);

    for (int i = 0; i < NUM_PLAYERS; i++) {
        initPlayer(&players[i], playerIds[i], availableRoles[i]);
        players[i].sentVoteId = -1; 
    }
}

const char* getRoleName(Role r) {
    switch (r) {
        case ROLE_CITIZEN: return "Citizen";
        case ROLE_MAFIA:   return "Mafia";
        case ROLE_POLICE:  return "Police";
        case ROLE_DOCTOR:  return "Doctor";
        default:           return "Unknown";
    }
}