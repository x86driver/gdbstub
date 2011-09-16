#include <stdio.h>
#include "wrapper.h"

static uint32_t reg_table[CPU_GP_REGS + 1] = {  /* plus CPSR */
    0, 0x408003f8, 0, 0,
    0, 0, 0, 0,
    0, 0, 0x8294, 0,
    0, 0x400802a8, 0, 0x8204,
    0x10
};

uint32_t get_reg(struct CPUState *env, int reg)
{
    env = env;
    return reg_table[reg];
}

uint32_t get_mem(struct CPUState *env, uint32_t addr)
{
    env = env;
    printf("get mem: 0x%x\n", addr);
    return 0;
}
