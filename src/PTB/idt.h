#pragma once
#include <stdint.h>

void idt_init(void);
void idt_set_gate(uint8_t vec, void (*handler)(void));
uint64_t ticks_get(void);

struct intr_frame;
void isr_spurious(struct intr_frame*);
void isr_lapic_timer(struct intr_frame*);
void isr_kbd(struct intr_frame*);