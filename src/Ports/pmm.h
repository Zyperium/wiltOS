#pragma once
#include <stdint.h>
#include <limine.h>

void pmm_init(struct limine_memmap_response *mm, uint64_t kphys, uint64_t ksize);
uint64_t pmm_alloc(void);
void pmm_free(uint64_t paddr);
uint64_t pmm_total(void);
uint64_t pmm_used(void);
