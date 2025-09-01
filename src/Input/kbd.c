#include <stdint.h>
#include "../PTB/io.h"
#include "kbd.h"

#define KBD_QSZ 64

static volatile char q[KBD_QSZ];
static volatile uint8_t qh, qt;
static uint8_t shift, caps, ext;

static inline void wait_in(void){ while (inb(0x64) & 0x02) {} }
static inline void wait_out(void){ while ((inb(0x64) & 0x01) == 0) {} }

static const char map_norm[128] = {
 [0x02]='1',[0x03]='2',[0x04]='3',[0x05]='4',[0x06]='5',[0x07]='6',[0x08]='7',[0x09]='8',[0x0A]='9',[0x0B]='0',
 [0x0C]='-',[0x0D]='=',[0x0E]=8,[0x0F]='\t',
 [0x10]='q',[0x11]='w',[0x12]='e',[0x13]='r',[0x14]='t',[0x15]='y',[0x16]='u',[0x17]='i',[0x18]='o',[0x19]='p',
 [0x1A]='[',[0x1B]=']',[0x1C]='\n',
 [0x1E]='a',[0x1F]='s',[0x20]='d',[0x21]='f',[0x22]='g',[0x23]='h',[0x24]='j',[0x25]='k',[0x26]='l',
 [0x27]=';',[0x28]='\'',[0x29]='`',[0x2B]='\\',
 [0x2C]='z',[0x2D]='x',[0x2E]='c',[0x2F]='v',[0x30]='b',[0x31]='n',[0x32]='m',
 [0x33]=',',[0x34]='.',[0x35]='/',[0x39]=' '
};
static const char map_shift[128] = {
 [0x02]='!',[0x03]='@',[0x04]='#',[0x05]='$',[0x06]='%',[0x07]='^',[0x08]='&',[0x09]='*',[0x0A]='(',[0x0B]=')',
 [0x0C]='_',[0x0D]='+',
 [0x10]='Q',[0x11]='W',[0x12]='E',[0x13]='R',[0x14]='T',[0x15]='Y',[0x16]='U',[0x17]='I',[0x18]='O',[0x19]='P',
 [0x1A]='{',[0x1B]='}',[0x1C]='\n',
 [0x1E]='A',[0x1F]='S',[0x20]='D',[0x21]='F',[0x22]='G',[0x23]='H',[0x24]='J',[0x25]='K',[0x26]='L',
 [0x27]=':',[0x28]='"',[0x29]='~',[0x2B]='|',
 [0x2C]='Z',[0x2D]='X',[0x2E]='C',[0x2F]='V',[0x30]='B',[0x31]='N',[0x32]='M',
 [0x33]='<',[0x34]='>',[0x35]='?',[0x39]=' '
};

static inline void qpush(char c){
    uint8_t nx = (uint8_t)((qh + 1) & (KBD_QSZ - 1));
    if (nx != qt){ q[qh] = c; qh = nx; }
}

void kbd_init(void){
    qh = qt = 0; shift = caps = ext = 0;

    wait_in(); outb(0x64, 0xAD);
    wait_in(); outb(0x64, 0xA7);
    while (inb(0x64) & 0x01) (void)inb(0x60);

    wait_in(); outb(0x64, 0x20);
    wait_out(); uint8_t cfg = inb(0x60);
    cfg |= 0x01;
    cfg &= (uint8_t)~0x10;
    wait_in(); outb(0x64, 0x60);
    wait_in(); outb(0x60, cfg);

    wait_in(); outb(0x64, 0xAE);

    wait_in(); outb(0x60, 0xF4);
    wait_out(); (void)inb(0x60);
}

int kbd_getch(void){
    if (qh == qt) return -1;
    char c = q[qt]; qt = (uint8_t)((qt + 1) & (KBD_QSZ - 1));
    return (int)(unsigned char)c;
}

extern void serial_write(const char*);
extern void serial_puthex64(uint64_t);

static void kbd_irq(void){
    uint8_t st = inb(0x64);
    if ((st & 0x01) == 0) return;
    uint8_t sc = inb(0x60);

    if (sc == 0xE0){ ext = 1; return; }
    uint8_t rel = sc & 0x80; sc &= 0x7F;

    if (sc == 0x2A || sc == 0x36){ shift = rel ? 0 : 1; return; }
    if (sc == 0x3A){ if (!rel) caps ^= 1; return; }
    if (rel) return;

    char c0 = map_norm[sc];
    char c1 = map_shift[sc];
    char out = 0;

    if (c0 >= 'a' && c0 <= 'z') out = (shift ^ caps) ? (char)(c0 - 32) : c0;
    else out = shift ? (c1 ? c1 : c0) : c0;

    if (!out) return;

    qpush(out);
    ext = 0;
}

void kbd_isr_poll(void){ kbd_irq(); }

void kbd_ps2_drain(void){
    for (int i=0;i<256;i++){
        if (inb(0x64) & 1) (void)inb(0x60);
        else break;
    }
}