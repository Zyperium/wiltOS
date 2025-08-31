#pragma once
#include <stdint.h>

void pic_disable(void);

void apic_init(void);
void lapic_eoi(void);
uint32_t lapic_id(void);

void ioapic_init(uint64_t ioapic_phys);
void ioapic_route_gsi(uint32_t gsi, uint8_t vector, int level, int active_low);

void lapic_timer_init(uint8_t vector, uint32_t initial_count, uint8_t div_pow2);