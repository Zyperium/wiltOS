#include <stdint.h>
#include "io.h"

#define COM1 0x3F8

static int serial_ready = 0;
static int tx_ready(void) { return inb(COM1 + 5) & 0x20; }

void serial_init(void) {
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
    serial_ready = 1;
}

void serial_putc(char c) {
    if (!serial_ready) return;
    while (!tx_ready()) {}
    outb(COM1, (uint8_t)c);
}

void serial_putu64(uint64_t x){
    char buf[21];
    int i = 0;
    if (x == 0){ serial_putc('0'); return; }
    while (x){ buf[i++] = (char)('0' + (x % 10)); x /= 10; }
    while (i--) serial_putc(buf[i]);
}

void serial_puti64(int64_t v){
    if (v < 0){ serial_putc('-'); serial_putu64((uint64_t)(-v)); }
    else serial_putu64((uint64_t)v);
}

void serial_write(const char *s) {
    while (*s) serial_putc(*s++);
}

void serial_puthex64(uint64_t x) {
    static const char *H = "0123456789ABCDEF";
    for (int i = 60; i >= 0; i -= 4) serial_putc(H[(x >> i) & 0xF]);
}
