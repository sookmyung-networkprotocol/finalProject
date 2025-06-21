#pragma once

#include <stdint.h>

// FSM 초기화 (ID 설정 및 시리얼 인터페이스 설정)
void L3_initFSM(uint8_t myId, uint8_t destId);

// FSM 상태 실행 루프 (main loop에서 주기적으로 호출)
void L3_FSMrun(void);
