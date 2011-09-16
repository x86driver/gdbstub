#ifndef _WRAPPER_H
#define _WRAPPER_H

#include <stdint.h>
#include "gdbstub.h"

enum {
    REG_R0 = 0, REG_R1, REG_R2, REG_R3,
    REG_R4, REG_R5, REG_R6, REG_R7,
    REG_R8, REG_R9, REG_SL, REG_FP,
    REG_IP, REG_SP, REG_LR, REG_PC
};

struct CPUState {
};

uint32_t get_reg(struct CPUState *env, int reg);
uint32_t get_mem(struct CPUState *env, uint32_t addr);
void set_reg(struct CPUState *env, int reg, uint32_t val);

#endif
