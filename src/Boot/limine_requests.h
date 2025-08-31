#pragma once
#include <stdint.h>
#include <limine.h>

extern volatile struct limine_framebuffer_request fb_req;
extern volatile struct limine_memmap_request     memmap_req;
extern volatile struct limine_kernel_address_request kaddr_req;
extern volatile struct limine_hhdm_request       hhdm_req;

static inline uint64_t HHDM_offset(void){
    return hhdm_req.response ? hhdm_req.response->offset : 0;
}
