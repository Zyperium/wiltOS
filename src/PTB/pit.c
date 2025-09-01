// src/pit.c
#include <stdint.h>
#include "io.h"

void pit_init(uint32_t hz){
    uint32_t div = 1193182u / (hz ? hz : 100u);
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(div & 0xFF));
    outb(0x40, (uint8_t)(div >> 8));
}
