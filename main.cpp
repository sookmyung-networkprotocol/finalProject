// main.cpp

#include "mbed.h"
#include "string.h"
#include "L2_FSMmain.h"
#include "L3_FSMmain.h"
#include "L3_shared.h"  // extern Serial pc 포함

uint8_t input_thisId = 1;  // 기본값: 호스트 ID
uint8_t input_destId = 0;  // 게스트는 hostId = 1로 설정할 것

int main(void)
{
    pc.printf("\n------------------ protocol stack starts! --------------------------\n");

    // 사용자로부터 ID 입력
    pc.printf(":: ID for this node : ");
    pc.scanf("%hhu", &input_thisId);
    pc.printf(":: ID for the destination : ");
    pc.scanf("%hhu", &input_destId);
    pc.getc();  // 개행 문자 제거

    pc.printf("endnode : %d, dest : %d\n\n", input_thisId, input_destId);

    // FSM 초기화
    L2_initFSM(input_thisId);
    L3_initFSM(input_thisId, input_destId);

    // 프로토콜 스택 루프
    while (1)
    {
        L2_FSMrun();
        L3_FSMrun();
    }
}
