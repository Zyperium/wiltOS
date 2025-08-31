#pragma once
#include <stdint.h>
typedef struct { uint64_t lba_start; uint64_t sectors; } part_t;
int mbr_find_fat32(part_t* out);
