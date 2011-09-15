#include <stdio.h>
#include "wrapper.h"

static uint32_t reg_table[CPU_REGS] = {
    0x12345678, 0xfedcba98, 0, 0,
    0, 0, 0, 0,
    0, 0x8000, 0x9000, 0xf0000000,
    0, 0, 0x8110, 0x9200,
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
