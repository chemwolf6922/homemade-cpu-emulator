#include "cpu.h"
#include "stdint.h"
#include "stdlib.h"
#include "string.h"

#if defined DEBUG || defined DEBUG_PC
#include "stdio.h"
#endif

static uint32_t FLAG = 0x00000000;
enum FLAGS
{
    // PC does not increment in the next round
    F_PC_HALT,
    // PC does a double increment in the next round (for 32bits instructions)
    F_PC_DOUBLE,
    // current operation wirtes new value to register
    F_STORE_REG,
    F_WFI,
    F_HALT
};
uint32_t HWGetFlag(uint32_t flag)
{
    return FLAG >> flag & 1U;
}
void HWSetFlag(uint32_t flag)
{
    FLAG |= 1U << flag;
}
void HWResetFlag(uint32_t flag)
{
    FLAG = ~(~FLAG | 1U << flag);
}

static uint32_t REG[8];
enum REGS
{
    REG0,
    REG1,
    REG2,
    REG3,
    REG4,
    REG5,
    SP,
    PC
};

#define MEMSIZE 65536
static uint8_t MEM[MEMSIZE];
enum MEMOFFSETS
{
    MEM_ROM = 0x0000,
    // program for handling interrupt, may need to expend the space
    P_INTERRUPT = 0x8000,
    MEM_USER = 0x8100,
    STACK_BASE = 0xFEFF,
    MEM_HW = 0xFF00
};

static uint16_t IMUSK = 0x0000;
static uint16_t ISTATE = 0x0000;
enum INTERRUPTS
{
    IFAULT,
    I1,
    I2,
    I3,
    I4,
    I5,
    I6,
    I7,
    I8,
    I9,
    I10,
    I11,
    I12,
    I13,
    I14,
    I15
};
// unlike flags, interrupt needs to be reset in software
void HWSetInterrupt(int interrupt)
{
    ISTATE |= 1U << interrupt;
}

void HWReset(void)
{
    memset(MEM, 0x00, MEMSIZE);
    // set ifault musk bit/ bit 0
    IMUSK = 0x0001;
    memset(REG, 0x00, 24);
    REG[SP] = STACK_BASE;
    REG[PC] = 0x0000;
    HWSetFlag(F_PC_HALT);
}

void decodeValues(uint32_t *v1, uint32_t *v2, uint8_t r1, uint8_t r2, uint8_t imMode, uint16_t im)
{
    *v1 = REG[r1];
    if (imMode == 0b00)
    {
        *v2 = REG[r2];
    }
    else if (imMode == 0b01)
    {
        if (r2 & 0b100)
        {
            // negative im
            *v2 = 0xFFFFFFF8 | r2;
        }
        else
        {
            *v2 = r2;
        }
    }
    else if (imMode == 0b10)
    {
        HWSetFlag(F_PC_DOUBLE);
        if (im & 0x8000)
        {
            *v2 = 0xFFFF0000 | im;
        }
        else
        {
            *v2 = im;
        }
    }
    else
    {
        HWSetInterrupt(IFAULT);
    }
}

uint32_t memOperation(uint8_t op, uint32_t v1, uint32_t v2)
{
    uint32_t result;
    switch (op)
    {
    case 0x0:
        result = *(uint8_t *)(MEM + v1);
        HWSetFlag(F_STORE_REG);
        break;
    case 0x1:
        result = *(uint16_t *)(MEM + v1);
        HWSetFlag(F_STORE_REG);
        break;
    case 0x2:
        result = *(uint32_t *)(MEM + v1);
        HWSetFlag(F_STORE_REG);
        break;
    case 0x3:
        *(uint8_t *)(MEM + v1) = (uint8_t)v2;
        break;
    case 0x4:
        *(uint16_t *)(MEM + v1) = (uint16_t)v2;
        break;
    case 0x5:
        *(uint32_t *)(MEM + v1) = v2;
        break;
    case 0x6:
        *(uint32_t *)(MEM + REG[SP]) = v2;
        REG[SP] += 4;
        break;
    case 0x7:
        result = *(uint32_t *)(MEM + REG[SP]);
        REG[SP] -= 4;
        HWSetFlag(F_STORE_REG);
        break;
        // no defult
    }
}

uint32_t alu(uint8_t op, uint32_t v1, uint32_t v2)
{
    static uint32_t carry = 0;
    uint32_t result;
    switch (op)
    {
    case 0x10:
    {
        uint64_t temp = (uint64_t)v1 + (uint64_t)v2 + (uint64_t)carry;
        result = temp & 0xFFFFFFFF;
        carry = (temp & 0x100000000UL) >> 32;
    };
    break;
    case 0x11:
    {
        uint64_t temp = (uint64_t)v1 + (uint64_t)(~v2) + (uint64_t)(!carry);
        result = temp & 0xFFFFFFFF;
        carry = (temp & 0x100000000UL) >> 32;
    };
    break;
    case 0x12:
        result = v1 > v2 ? 1 : v1 == v2 ? 0 : 0xFFFFFFFF;
        break;
    case 0x13:
        result = v1 & v2;
        break;
    case 0x14:
        result = v1 | v2;
        break;
    case 0x15:
        result = ~v1;
        break;
    case 0x16:
        result = v1 >> v2;
        break;
    case 0x17:
        result = v1 << v2;
        break;
    case 0x18:
        result = (uint32_t)((int32_t)v1 >> v2);
        break;
    case 0x19:
        result = v1 >> v2 | v1 << (32 - v2);
        break;
    default:
        result = 0;
        HWSetInterrupt(IFAULT);
        break;
    }
    HWSetFlag(F_STORE_REG);
    return result;
}

uint32_t flowControl(uint8_t op, uint32_t v1, uint32_t v2)
{
    uint32_t result;
    switch (op)
    {
    case 0x8:
        /* code */
        if (v1)
        {
            REG[PC] = v2;
            HWSetFlag(F_PC_HALT);
        }
        break;
    case 0x9:
        // todo: handle interrupt
        HWSetFlag(F_WFI);
        break;
    case 0xA:
        //v2: 0 | 1
        if (v2)
        {
            IMUSK |= 1U << v1;
        }
        else
        {
            IMUSK = ~(~IMUSK | 1U << v1);
        }
        break;
    case 0xB:
        result = IMUSK >> v1 & 0x1U;
        HWSetFlag(F_STORE_REG);
        break;
    case 0xC:
        if (v2)
        {
            ISTATE |= 1U << v1;
        }
        else
        {
            ISTATE = ~(~ISTATE | 1U << v1);
        }
        break;
    case 0xD:
        result = IMUSK >> v1 & 0x1U;
        HWSetFlag(F_STORE_REG);
        break;
    case 0xE:
        HWSetFlag(F_HALT);
        break;
    case 0xF:
        break;
    default:
        HWSetInterrupt(IFAULT);
        break;
    }
}

void storeValue(uint8_t rdst, uint32_t v)
{
    switch (rdst)
    {
    case REG0:
        break;
    case REG1:
    case REG2:
    case REG3:
    case REG4:
    case REG5:
    case SP:
        REG[rdst] = v;
        break;
    case PC:
        REG[rdst] = v;
        HWSetFlag(F_PC_HALT);
        break;
    default:
        break;
    }
}

void handleInterrupt()
{
    // push current PC to stack
    *(uint32_t *)(MEM + REG[SP]) = REG[PC];
    REG[SP] -= 4;
    // goto interrupt handler
    REG[PC] = P_INTERRUPT;
    HWSetFlag(F_PC_HALT);
    HWResetFlag(F_WFI);
}

void evalInstruction(uint8_t op, uint8_t rdst, uint8_t r1, uint8_t r2, uint8_t imMode, uint16_t im)
{
    static uint32_t v1 = 0;
    static uint32_t v2 = 0;
    static uint32_t result = 0;
    // clear bits
    op = op & 0x1F;
    rdst = rdst & 0x7;
    r1 = r1 & 0x7;
    r2 = r2 & 0x7;
    imMode = imMode & 0x3;
    im = im & 0xFFFF;

    // TODO handle interrupts
    if (IMUSK & ISTATE)
    {
        handleInterrupt();
        return;
    }

    // WFI flag
    if (HWGetFlag(F_WFI))
    {
        return;
    }

    // stage 1: get values
    decodeValues(&v1, &v2, r1, r2, imMode, im);

#ifdef DEBUG
    printf("OP code and variables: \r\n");
    printf("op:%d,v1:%d,v2:%d\r\n", op, v1, v2);
#endif

    // stage 2: calculate
    if (op < 8)
    {
        result = memOperation(op, v1, v2);
    }
    else if (op < 0xF)
    {
        result = flowControl(op, v1, v2);
    }
    else
    {
        result = alu(op, v1, v2);
    }

#ifdef DEBUG
    printf("ALU result:\r\n");
    printf("Result: %d\r\n", result);
#endif

    // state 3: store values
    if (HWGetFlag(F_STORE_REG))
    {
        storeValue(rdst, result);
    }

#ifdef DEBUG
    printf("REG & FLAG status\r\n");
    printf("REG0: %x\r\n", REG[REG0]);
    printf("REG1: %u\t%d\t%x\r\n", REG[REG1], REG[REG1], REG[REG1]);
    printf("REG2: %u\t%d\t%x\r\n", REG[REG2], REG[REG2], REG[REG2]);
    printf("REG3: %u\t%d\t%x\r\n", REG[REG3], REG[REG3], REG[REG3]);
    printf("REG4: %u\t%d\t%x\r\n", REG[REG4], REG[REG4], REG[REG4]);
    printf("REG5: %u\t%d\t%x\r\n", REG[REG5], REG[REG5], REG[REG5]);
    printf("SP: %x\r\n", REG[SP]);
    printf("PC: %x\r\n", REG[PC]);
    printf("FLAGS: %x\r\n\r\n", FLAG);
#endif
}

void cpuLoop(void)
{
    while (1)
    {
        // update pc
        if (HWGetFlag(F_HALT))
        {
            break;
            // more like continue, but the simulation needs to stop
        }
#ifdef DEBUG_PC
        printf("PC: %d\r\n",REG[PC]);
#endif
        if (!HWGetFlag(F_PC_HALT))
        {
            if (HWGetFlag(F_PC_DOUBLE))
            {
                REG[PC] += 4;
            }
            else
            {
                REG[PC] += 2;
            }
            if (REG[PC] >= MEMSIZE - 3)
            {
                HWSetFlag(F_HALT);
            }
        }
        HWResetFlag(F_PC_HALT);
        HWResetFlag(F_PC_DOUBLE);
        uint16_t high16 = *(uint16_t *)(MEM + REG[PC]);
        uint16_t low16 = *(uint16_t *)(MEM + REG[PC] + 2);
        evalInstruction(high16 >> 11 & 0b11111U, high16 >> 8 & 0b111U, high16 >> 5 & 0b111U, high16 >> 2 & 0x111U, high16 & 0b11U, low16);
    }
}


// burn in the program
void loadProgram(const char* program,int len)
{
    memcpy(MEM,program,len);
}