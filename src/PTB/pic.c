#include <stdint.h>
#include "io.h"

void pic_setmask(uint16_t m){
    outb(0x21, (uint8_t)(m & 0xFF));
    outb(0xA1, (uint8_t)((m >> 8) & 0xFF));
}

void pic_remap(int o1, int o2){
    uint8_t a1 = inb(0x21);
    uint8_t a2 = inb(0xA1);
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, (uint8_t)o1);
    outb(0xA1, (uint8_t)o2);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, a1);
    outb(0xA1, a2);
}

void pic_eoi(int irq){
    if (irq >= 8) outb(0xA0, 0x20);
    outb(0x20, 0x20);
}
