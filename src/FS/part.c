#include <stdint.h>
#include "part.h"
#include "../Block/block.h"

int mbr_find_fat32(part_t* out){
    uint8_t sec[512];
    if (block_get0()->read(0,1,sec)) return -1;
    if (sec[510]!=0x55 || sec[511]!=0xAA) return -1;
    for (int i=0;i<4;i++){
        const uint8_t* e = &sec[446 + i*16];
        uint8_t type = e[4];
        if (type==0x0B || type==0x0C){
            uint32_t lba = *(const uint32_t*)(e+8);
            uint32_t sz  = *(const uint32_t*)(e+12);
            out->lba_start = lba;
            out->sectors = sz;
            return 0;
        }
    }
    return -1;
}
