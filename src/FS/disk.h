#pragma once
#include <stdint.h>
#include "fat32.h"

void disk_setup(void);
int  disk_mounted(void);
fat32_t* disk_fs(void);
