#pragma once
#include <stdint.h>

int  ata_init(void);
int  ata_read(uint64_t lba, uint32_t cnt, void* buf);
int  ata_write(uint64_t lba, uint32_t cnt, const void* buf);

void ata_select_master(void);
void ata_select_slave(void);
int  ata_probe_current(void);
