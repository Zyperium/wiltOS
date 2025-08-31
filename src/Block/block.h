#pragma once
#include <stdint.h>
typedef struct {
    int (*read)(uint64_t lba, uint32_t cnt, void* buf);
    int (*write)(uint64_t lba, uint32_t cnt, const void* buf);
    uint32_t sector_size;
} blockdev_t;

int block_register(blockdev_t* dev);
blockdev_t* block_get0(void);
