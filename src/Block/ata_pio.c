#include <stdint.h>
#include "../PTB/io.h"
#include "block.h"
#include "ata_pio.h"

#define ATA_IO  0x1F0
#define ATA_CTL 0x3F6

static uint8_t devsel = 0xF0;  /* slave = data disk; your auto-probe may override */

static inline void io_wait400(void){ for(int i=0;i<4;i++) (void)inb(ATA_CTL); }

static int wait_bsy_clear_to(uint32_t spins){
    while (spins--){
        uint8_t s = inb(ATA_IO+7);
        if ((s & 0x80) == 0) return 0;
    }
    return -1;
}
static int wait_drq_set_to(uint32_t spins){
    while (spins--){
        uint8_t s = inb(ATA_IO+7);
        if (s & 0x01) return -2;
        if (s & 0x20) return -3;
        if (s & 0x08) return 0;
    }
    return -1;
}

void ata_select_master(void){ devsel = 0xE0; outb(ATA_IO+6, devsel); io_wait400(); }
void ata_select_slave(void){  devsel = 0xF0; outb(ATA_IO+6, devsel); io_wait400(); }

int ata_init(void){
    outb(ATA_CTL, 0x02);
    outb(ATA_IO+6, devsel);
    io_wait400();
    return 0;
}

int ata_read(uint64_t lba, uint32_t cnt, void* buf){
    if (!cnt) return 0;
    uint16_t* p = (uint16_t*)buf;
    while (cnt){
        uint8_t n = (cnt>255)? 0 : (uint8_t)cnt;
        outb(ATA_IO+6, devsel | ((lba>>24)&0x0F));
        outb(ATA_IO+2, n);
        outb(ATA_IO+3, (uint8_t)(lba));
        outb(ATA_IO+4, (uint8_t)(lba>>8));
        outb(ATA_IO+5, (uint8_t)(lba>>16));
        outb(ATA_IO+7, 0x20);
        uint32_t todo = (n? n:256);
        for (uint32_t s=0; s<todo; s++){
            if (wait_drq_set_to(1000000)) return -1;
            insw(ATA_IO, p, 256);
            p += 256;
        }
        cnt -= todo;
        lba += todo;
    }
    return 0;
}

int ata_write(uint64_t lba, uint32_t cnt, const void* buf){
    if (!cnt) return 0;
    const uint16_t* p = (const uint16_t*)buf;
    while (cnt){
        uint8_t n = (cnt>255)? 0 : (uint8_t)cnt;
        if (wait_bsy_clear_to(1000000)) return -1;
        outb(ATA_IO+6, devsel | ((lba>>24)&0x0F));
        outb(ATA_IO+2, n);
        outb(ATA_IO+3, (uint8_t)(lba));
        outb(ATA_IO+4, (uint8_t)(lba>>8));
        outb(ATA_IO+5, (uint8_t)(lba>>16));
        outb(ATA_IO+7, 0x30);
        uint32_t todo = (n? n:256);
        for (uint32_t s=0; s<todo; s++){
            if (wait_drq_set_to(1000000)) return -2;
            outsw(ATA_IO, p, 256);
            p += 256;
        }
        if (wait_bsy_clear_to(1000000)) return -3;
        outb(ATA_IO+7, 0xE7);
        if (wait_bsy_clear_to(1000000)) return -4;
        cnt -= todo;
        lba += todo;
    }
    return 0;
}

int ata_probe_current(void){
    uint8_t sec[512];
    if (ata_read(0,1,sec)) return -1;
    return (sec[510]==0x55 && sec[511]==0xAA) ? 0 : -1;
}
