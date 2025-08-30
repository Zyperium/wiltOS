#include <stdint.h>
#include "../Input/kbd.h"

struct idt_entry {
    uint16_t off0;
    uint16_t sel;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t off1;
    uint32_t off2;
    uint32_t zero;
} __attribute__((packed));

struct idt_ptr { uint16_t limit; uint64_t base; } __attribute__((packed));
typedef struct { uint64_t rip, cs, rflags, rsp, ss; } intr_frame_t;

extern void serial_write(const char*);
extern void serial_puthex64(uint64_t);
extern void hcf(void);
extern void pic_eoi(int irq);

static struct idt_entry idt[256];
static volatile uint64_t ticks;

static inline uint16_t get_cs(void){ uint16_t x; __asm__ __volatile__("mov %%cs,%0":"=r"(x)); return x; }
static inline void lidt(void* base, uint16_t size){
    struct idt_ptr d = { .limit = (uint16_t)(size - 1), .base = (uint64_t)base };
    __asm__ __volatile__("lidt %0" : : "m"(d));
}

static void set_gate(int vec, void* fn, uint8_t ta, uint8_t ist, uint16_t sel){
    uint64_t a = (uint64_t)fn;
    idt[vec].off0 = a & 0xFFFF;
    idt[vec].sel = sel;
    idt[vec].ist = ist;
    idt[vec].type_attr = ta;
    idt[vec].off1 = (a >> 16) & 0xFFFF;
    idt[vec].off2 = (uint32_t)(a >> 32);
    idt[vec].zero = 0;
}

__attribute__((interrupt))
static void isr_de(intr_frame_t* f){
    serial_write("ex:de rip=");
    serial_puthex64(f->rip);
    serial_write("\n");
    hcf();
}

__attribute__((interrupt))
static void isr_df(intr_frame_t* f, uint64_t ec){
    serial_write("ex:df rip=");
    serial_puthex64(f->rip);
    serial_write(" ec=");
    serial_puthex64(ec);
    serial_write("\n");
    hcf();
}

__attribute__((interrupt))
static void isr_gp(intr_frame_t* f, uint64_t ec){
    serial_write("ex:gp rip=");
    serial_puthex64(f->rip);
    serial_write(" ec=");
    serial_puthex64(ec);
    serial_write("\n");
    hcf();
}

__attribute__((interrupt))
static void isr_pf(intr_frame_t* f, uint64_t ec){
    uint64_t cr2; __asm__ __volatile__("mov %%cr2,%0":"=r"(cr2));
    serial_write("ex:pf rip=");
    serial_puthex64(f->rip);
    serial_write(" cr2=");
    serial_puthex64(cr2);
    serial_write(" ec=");
    serial_puthex64(ec);
    serial_write("\n");
    hcf();
}

__attribute__((interrupt))
static void isr_bp(intr_frame_t* f){
    serial_write("ex:bp rip=");
    serial_puthex64(f->rip);
    serial_write("\n");
}

__attribute__((interrupt))
static void isr_irq0(intr_frame_t* f){
    ++ticks;
    pic_eoi(0);
}

__attribute__((interrupt))
static void isr_irq1(intr_frame_t* f){
    kbd_irq();
    pic_eoi(1);
}

void idt_init(void){
    uint16_t cs = get_cs();
    for (int i = 0; i < 256; ++i) set_gate(i, hcf, 0x8E, 0, cs);
    set_gate(0,  isr_de,   0x8E, 0, cs);
    set_gate(8,  isr_df,   0x8E, 0, cs);
    set_gate(13, isr_gp,   0x8E, 0, cs);
    set_gate(14, isr_pf,   0x8E, 0, cs);
    set_gate(3,  isr_bp,   0x8E, 0, cs);
    set_gate(32, isr_irq0, 0x8E, 0, cs);
    set_gate(33, isr_irq1, 0x8E, 0, cs);
    lidt(idt, sizeof idt);
}

uint64_t ticks_get(void){ return ticks; }