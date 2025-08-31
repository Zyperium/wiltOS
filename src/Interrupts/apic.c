#include <stdint.h>
#include "../Boot/limine_requests.h"
#include "../MM/vmm.h"

static inline uint8_t inb(uint16_t p){ uint8_t v; __asm__ __volatile__("inb %1,%0":"=a"(v):"Nd"(p)); return v; }
static inline void outb(uint16_t p, uint8_t v){ __asm__ __volatile__("outb %0,%1"::"a"(v),"Nd"(p)); }
static inline uint64_t rdmsr(uint32_t id){ uint32_t lo,hi; __asm__ __volatile__("rdmsr":"=a"(lo),"=d"(hi):"c"(id)); return ((uint64_t)hi<<32)|lo; }
static inline void wrmsr(uint32_t id, uint64_t v){ __asm__ __volatile__("wrmsr"::"c"(id),"a"((uint32_t)v),"d"((uint32_t)(v>>32))); }

static volatile uint32_t *lapic;
static volatile uint32_t *ioapic_sel;
static volatile uint32_t *ioapic_win;

static inline void lapic_w(uint32_t off, uint32_t v){ lapic[off/4]=v; (void)lapic[off/4]; }
static inline uint32_t lapic_r(uint32_t off){ return lapic[off/4]; }
static inline void ioapic_w(uint32_t reg, uint32_t v){ *ioapic_sel = reg; *ioapic_win = v; }
static inline uint32_t ioapic_r(uint32_t reg){ *ioapic_sel = reg; return *ioapic_win; }

void pic_disable(void){ outb(0x21,0xFF); outb(0xA1,0xFF); }

void apic_init(void){
    uint64_t apic_base = rdmsr(0x1B);
    apic_base |= (1ull<<11);
    wrmsr(0x1B, apic_base);

    uint64_t pa = apic_base & ~0xFFFull;
    uint64_t va = HHDM_offset() + pa;
    vmm_map(pa, va, 0x1000, VMM_RW | VMM_NX);

    lapic = (volatile uint32_t*)va;
    lapic_w(0xF0, 0xFF | (1u<<8));
    lapic_w(0x80, 0x00);
    lapic_w(0x350, (1u<<16));
    lapic_w(0x360, (1u<<16));
    lapic_w(0x370, (1u<<16));
}

uint32_t lapic_id(void){ return lapic_r(0x20)>>24; }
void lapic_eoi(void){ lapic_w(0xB0, 0); }

void ioapic_init(uint64_t ioapic_phys){
    uint64_t pa = ioapic_phys & ~0xFFFull;
    uint64_t va = HHDM_offset() + pa;
    vmm_map(pa, va, 0x1000, VMM_RW | VMM_NX);

    ioapic_sel = (volatile uint32_t*)va;
    ioapic_win = (volatile uint32_t*)(va + 0x10);
    (void)ioapic_r(0x01);
}

void ioapic_route_gsi(uint32_t gsi, uint8_t vector, int level, int active_low){
    uint32_t idx = 0x10 + gsi*2;
    uint32_t lo = vector | ((active_low?1u:0u)<<13) | ((level?1u:0u)<<15);
    uint32_t hi = lapic_id()<<24;
    ioapic_w(idx+1, hi);
    ioapic_w(idx, lo & ~(1u<<16));
}

void lapic_timer_init(uint8_t vector, uint32_t initial_count, uint8_t div_pow2){
    static const uint32_t map[8]={0xB,0x0,0x1,0x2,0x3,0x8,0x9,0xA};
    uint32_t div = map[div_pow2 & 7];
    lapic_w(0x3E0, div);
    lapic_w(0x320, vector | (1u<<17));
    lapic_w(0x380, initial_count);
}
