#include "mbed.h"
#include "string.h"
#include "L2_FSMmain.h"
#include "L3_FSMmain.h"

//serial port interface
Serial pc(USBTX, USBRX);

//GLOBAL variables (DO NOT TOUCH!) ------------------------------------------
//source/destination ID
uint8_t input_thisId=1;
uint8_t input_destId=0;

//FSM operation implementation ------------------------------------------------
int main(void){

    //initialization
    pc.printf("------------------ protocol stack starts! --------------------------\n");
    //source & destination ID setting
    pc.printf(":: ID for this node : ");
    pc.scanf("%d", &input_thisId);
    pc.printf(":: ID for the destination : ");
    pc.scanf("%d", &input_destId);
    pc.getc();

    pc.printf("endnode : %i, dest : %i\n", input_thisId, input_destId);

    //initialize lower layer stacks
    L2_initFSM(input_thisId);
    L3_initFSM(input_thisId, input_destId);
    
    pc.printf("[DEBUG] Entering main loop\n");
    
    int loopCount = 0;
    while(1)
    {
        L2_FSMrun();
        L3_FSMrun();
        
        // 디버깅용 출력 (처음 10번만)
        if (loopCount < 10) {
            pc.printf("[DEBUG] Loop %d completed\n", loopCount++);
            wait(0.1); // 0.1초 대기
        }
    }
}