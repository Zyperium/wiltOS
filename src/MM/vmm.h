#pragma once
#include <stdint.h>

enum {
    VMM_RW   = 1u<<0,
    VMM_USER = 1u<<1,
    VMM_NX   = 1u<<2,
    VMM_G    = 1u<<3
};

void vmm_init(void);
int  vmm_map(uint64_t phys, uint64_t virt, uint64_t size, uint64_t flags);
int  vmm_unmap(uint64_t virt, uint64_t size, int free_phys);
uint64_t vmm_resolve(uint64_t virt, uint64_t *pte_out);
int vmm_set_writable(uint64_t virt, uint64_t size, int writable);
