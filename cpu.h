#ifndef __CPU_H
#define __CPU_H

#include "stdint.h"

#define DEBUG
// #define DEBUG_PC

void HWReset(void);

void loadProgram(const char* program,int len);

void cpuLoop(void);


#ifdef DEBUG
void evalInstruction(uint8_t op, uint8_t rdst, uint8_t r1, uint8_t r2, uint8_t imMode, uint16_t im);
#endif
// debug

#endif