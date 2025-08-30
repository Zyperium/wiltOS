#include <limine.h>
#include <stdint.h>
#include <stddef.h>
#include <limine.h>
#include "mmu.h"

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_req = {
    .id = LIMINE_HHDM_REQUEST, .revision = 0
};

static inline uint64_t read_cr3(void){ uint64_t v; __asm__ __volatile__("mov %%cr3,%0":"=r"(v)::); return v; }
static inline void invlpg(void* p){ __asm__ __volatile__("invlpg (%0)"::"r"(p):"memory"); }

static inline uint64_t hhdm(void){ return hhdm_req.response ? hhdm_req.response->offset : 0; }

#define NX  (1ull<<63)
#define PS  (1ull<<7)
#define P   (1ull<<0)

static void clear_nx_for_range(uint64_t va_start, uint64_t va_end){
    uint64_t pml4_phys = read_cr3() & ~0xfffull;
    uint64_t *pml4 = (uint64_t *)(hhdm() + pml4_phys);

    for (uint64_t va = va_start; va < va_end; ){
        uint64_t l4 = (va >> 39) & 0x1ff;
        uint64_t l3 = (va >> 30) & 0x1ff;
        uint64_t l2 = (va >> 21) & 0x1ff;
        uint64_t l1 = (va >> 12) & 0x1ff;

        uint64_t pml4e = pml4[l4];
        if (!(pml4e & P)) return;
        uint64_t *pdpt = (uint64_t *)(hhdm() + (pml4e & ~0xfffull));

        uint64_t pdpte = pdpt[l3];
        if (!(pdpte & P)) return;

        if (pdpte & PS){
            if (pdpte & NX){ pdpte &= ~NX; pdpt[l3] = pdpte; }
            uint64_t next = ((va >> 30) + 1) << 30;
            invlpg((void*)va);
            va = next;
            continue;
        }

        uint64_t *pd = (uint64_t *)(hhdm() + (pdpte & ~0xfffull));
        uint64_t pde = pd[l2];
        if (!(pde & P)) return;

        if (pde & PS){
            if (pde & NX){ pde &= ~NX; pd[l2] = pde; }
            uint64_t next = ((va >> 21) + 1) << 21;
            invlpg((void*)va);
            va = next;
            continue;
        }

        uint64_t *pt = (uint64_t *)(hhdm() + (pde & ~0xfffull));
        for (uint64_t i = l1; i < 512 && va < va_end; i++){
            uint64_t pte = pt[i];
            if (pte & P){
                if (pte & NX){ pte &= ~NX; pt[i] = pte; invlpg((void*)va); }
            }
            va += 4096;
        }
    }
}

void mmu_mark_executable(void* addr, uint64_t len){
    uint64_t start = (uint64_t)addr & ~0xfffull;
    uint64_t end   = (uint64_t)addr + len;
    end = (end + 0xfff) & ~0xfffull;
    clear_nx_for_range(start, end);
}
