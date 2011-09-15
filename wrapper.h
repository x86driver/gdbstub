#ifndef _WRAPPER_H
#define _WRAPPER_H

#include <stdint.h>
#include "gdbstub.h"

struct CPUState {
};

uint32_t get_reg(struct CPUState *env, int reg);
uint32_t get_mem(struct CPUState *env, uint32_t addr);

#endif
