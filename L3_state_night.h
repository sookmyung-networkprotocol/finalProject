// L3_state_night.h
#pragma once

#include "L3_host.h"

// 밤 역할별 FSM 처리 핸들러
void L3_handleMafia();     // 마피아가 죽일 대상 선택
void L3_handleDoctor();    // 의사가 살릴 대상 선택
void L3_handlePolice();    // 경찰이 조사할 대상 선택 및 결과 알림

// 도우미 함수들
bool isRoleDead(Role role);    // 해당 역할의 생존 여부 확인
void sendAliveListToRoleWithExclusion(Role role);  // 해당 역할을 제외한 생존자 목록 전송
