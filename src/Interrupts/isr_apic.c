#include <stdint.h>
#include "apic.h"

struct intr_frame { uint64_t rip, cs, rflags, rsp, ss; };

volatile uint64_t ticks;

extern void kbd_isr_poll(void);
