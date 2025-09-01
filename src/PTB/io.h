#pragma once
#include <stdint.h>
#include <stddef.h>

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__("outb %0,%1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ __volatile__("inb %1,%0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void insw(uint16_t port, void *dst, size_t cnt){
    __asm__ volatile("rep insw"
                     : "+D"(dst), "+c"(cnt)
                     : "d"(port)
                     : "memory");
}
static inline void outsw(uint16_t port, const void *src, size_t cnt){
    __asm__ volatile("rep outsw"
                     : "+S"(src), "+c"(cnt)
                     : "d"(port)
                     : "memory");
}