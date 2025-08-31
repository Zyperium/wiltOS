#include "../Block/block.h"
#include "../Block/ata_pio.h"
#include "part.h"
#include "fat32.h"
#include "disk.h"

static fat32_t gfs;
static int g_mounted = 0;

void disk_setup(void){
    static blockdev_t ata = { ata_read, ata_write, 512 };
    ata_init();
    block_register(&ata);

    ata_select_slave();
    if (ata_probe_current() != 0){
        ata_select_master();
        if (ata_probe_current() != 0){
            g_mounted = 0;
            return;
        }
    }

    part_t p;
    if (mbr_find_fat32(&p)==0 && fat32_mount(&gfs, block_get0(), p.lba_start)==0)
        g_mounted = 1;
    else
        g_mounted = 0;
}

int  disk_mounted(void){ return g_mounted; }
fat32_t* disk_fs(void){ return g_mounted ? &gfs : 0; }
