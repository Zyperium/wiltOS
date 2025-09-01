#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../Block/block.h"

typedef struct {
    blockdev_t* b;
    uint32_t bytes_per_sec;
    uint32_t sec_per_clus;
    uint32_t rsvd_secs;
    uint32_t num_fats;
    uint32_t fat_sz;
    uint32_t root_clus;
    uint64_t part_lba;
    uint64_t fat_lba;
    uint64_t data_lba;
} fat32_t;

int fat32_mount(fat32_t* fs, blockdev_t* b, uint64_t part_lba);
int fat32_read_all(fat32_t* fs, const char* path, uint8_t** out, uint32_t* sz);
int fat32_write_all_root(fat32_t* fs, const char* name83, const uint8_t* data, uint32_t sz);
int fat32_list_root(fat32_t* fs, void (*cb)(const char* name, uint32_t size, int is_dir));
int fat32_read_stream(fat32_t* fs, const char* name83,
                      int (*sink)(const uint8_t* data, uint32_t len));
int fat_get(fat32_t*, uint32_t, uint32_t*);