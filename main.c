#include "cpu.h"
#include "stdio.h"
#include "stdint.h"
#include "string.h"

const uint16_t program[] = {0b1000000100000010,1000,0b0111000000000000};


int main(void)
{
    HWReset();
    loadProgram((const uint8_t*)program,sizeof(program));
    cpuLoop();
    return 0;
}