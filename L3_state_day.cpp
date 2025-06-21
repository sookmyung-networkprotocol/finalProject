#include "L3_state_day.h"
#include "L3_shared.h"
#include "mbed.h"

extern Serial pc;

void L3_handleDay() {
    pc.printf("\r\n낮이 되었습니다.\n\n");

    // TODO: 단체 채팅 구현이 있다면 여기에 추가

    change_state = 0;
    main_state = VOTE;
}
