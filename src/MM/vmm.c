#include <stdint.h>
#include <stddef.h>
#include <limine.h>
#include "vmm.h"
#include "pmm.h"
#include "../Boot/limine_requests.h"

static inline uint64_t rdcr3(void){ uint64_t v; __asm__ __volatile__("mov %%cr3,%0":"=r"(v)); return v; }
static inline void invlpg(void* p){ __asm__ __volatile__("invlpg (%0)"::"r"(p):"memory"); }
static inline uint64_t HHDM(void){ return hhdm_req.response ? hhdm_req.response->offset : 0; }

#define PTE_P  (1ull<<0)
#define PTE_W  (1ull<<1)
#define PTE_U  (1ull<<2)
#define PTE_G  (1ull<<8)
#define PTE_PS (1ull<<7)
#define PTE_NX (1ull<<63)

static inline uint64_t* phys_to_virt(uint64_t pa){ return (uint64_t*)(HHDM() + (pa & ~0xfffull)); }

static uint64_t ensure_table(uint64_t *table, uint64_t idx){
    uint64_t e = table[idx];
    if (e & PTE_P) return e & ~0xfffull;
    uint64_t pa = pmm_alloc();
    uint64_t *va = phys_to_virt(pa);
    for (int i=0;i<512*8/8;i++) ((uint64_t*)va)[i]=0;
    table[idx] = pa | PTE_P | PTE_W | PTE_U;
    return pa;
}

void vmm_init(void){ (void)HHDM(); }

static inline void flags_to_bits(uint64_t flags, uint64_t *lo, uint64_t *nx){
    uint64_t b = PTE_P;
    if (flags & VMM_RW)   b |= PTE_W;
    if (flags & VMM_USER) b |= PTE_U;
    if (flags & VMM_G)    b |= PTE_G;
    *lo = b;
    *nx = (flags & VMM_NX) ? PTE_NX : 0;
}

int vmm_map(uint64_t phys, uint64_t virt, uint64_t size, uint64_t flags){
    uint64_t pml4_pa = rdcr3() & ~0xfffull;
    uint64_t *pml4 = phys_to_virt(pml4_pa);
    uint64_t start = virt & ~0xfffull;
    uint64_t end = (virt + size + 0xfff) & ~0xfffull;
    uint64_t pa = phys & ~0xfffull;
    uint64_t lo, nx; flags_to_bits(flags, &lo, &nx);

    for (uint64_t va = start; va < end; va += 0x1000, pa += 0x1000){
        uint64_t i4 = (va >> 39) & 0x1ff;
        uint64_t i3 = (va >> 30) & 0x1ff;
        uint64_t i2 = (va >> 21) & 0x1ff;
        uint64_t i1 = (va >> 12) & 0x1ff;

        uint64_t pdpt_pa = ensure_table(pml4, i4);
        uint64_t *pdpt = phys_to_virt(pdpt_pa);
        uint64_t pd_pa = ensure_table(pdpt, i3);
        uint64_t *pd = phys_to_virt(pd_pa);
        uint64_t pt_pa = ensure_table(pd, i2);
        uint64_t *pt = phys_to_virt(pt_pa);

        uint64_t e = (pt[i1] & ~0xfffull) | lo;
        pt[i1] = pa | (lo) | (nx);
        invlpg((void*)va);
    }
    return 0;
}

int vmm_set_writable(uint64_t virt, uint64_t size, int writable){
    uint64_t pml4_pa = rdcr3() & ~0xfffull;
    uint64_t *pml4 = phys_to_virt(pml4_pa);
    uint64_t start = virt & ~0xfffull;
    uint64_t end   = (virt + size + 0xfff) & ~0xfffull;
    for (uint64_t va = start; va < end; va += 0x1000){
        uint64_t i4 = (va >> 39) & 0x1ff;
        uint64_t i3 = (va >> 30) & 0x1ff;
        uint64_t i2 = (va >> 21) & 0x1ff;
        uint64_t i1 = (va >> 12) & 0x1ff;

        uint64_t pml4e = pml4[i4]; if (!(pml4e & PTE_P)) return -1;
        uint64_t *pdpt = phys_to_virt(pml4e);
        uint64_t pdpte = pdpt[i3]; if (!(pdpte & PTE_P)) return -1;
        if (pdpte & PTE_PS) return -1;
        uint64_t *pd = phys_to_virt(pdpte);
        uint64_t pde = pd[i2]; if (!(pde & PTE_P)) return -1;
        if (pde & PTE_PS) return -1;
        uint64_t *pt = phys_to_virt(pde);
        uint64_t pte = pt[i1]; if (!(pte & PTE_P)) return -1;

        if (writable) pt[i1] = pte | PTE_W; else pt[i1] = pte & ~PTE_W;
        invlpg((void*)va);
    }
    return 0;
}

int vmm_unmap(uint64_t virt, uint64_t size, int free_phys){
    uint64_t pml4_pa = rdcr3() & ~0xfffull;
    uint64_t *pml4 = phys_to_virt(pml4_pa);
    uint64_t start = virt & ~0xfffull;
    uint64_t end = (virt + size + 0xfff) & ~0xfffull;

    for (uint64_t va = start; va < end; va += 0x1000){
        uint64_t i4 = (va >> 39) & 0x1ff;
        uint64_t i3 = (va >> 30) & 0x1ff;
        uint64_t i2 = (va >> 21) & 0x1ff;
        uint64_t i1 = (va >> 12) & 0x1ff;

        uint64_t pml4e = pml4[i4];
        if (!(pml4e & PTE_P)) continue;
        uint64_t *pdpt = phys_to_virt(pml4e);
        uint64_t pdpte = pdpt[i3];
        if (!(pdpte & PTE_P)) continue;
        if (pdpte & PTE_PS){ invlpg((void*)va); continue; }
        uint64_t *pd = phys_to_virt(pdpte);
        uint64_t pde = pd[i2];
        if (!(pde & PTE_P)) continue;
        if (pde & PTE_PS){ invlpg((void*)va); continue; }
        uint64_t *pt = phys_to_virt(pde);
        uint64_t pte = pt[i1];
        if (pte & PTE_P){
            if (free_phys) pmm_free(pte & ~0xfffull);
            pt[i1] = 0;
            invlpg((void*)va);
        }
    }
    return 0;
}

uint64_t vmm_resolve(uint64_t virt, uint64_t *pte_out){
    uint64_t pml4_pa = rdcr3() & ~0xfffull;
    uint64_t *pml4 = phys_to_virt(pml4_pa);
    uint64_t i4 = (virt >> 39) & 0x1ff;
    uint64_t i3 = (virt >> 30) & 0x1ff;
    uint64_t i2 = (virt >> 21) & 0x1ff;
    uint64_t i1 = (virt >> 12) & 0x1ff;

    uint64_t pml4e = pml4[i4]; if (!(pml4e & PTE_P)) return 0;
    uint64_t *pdpt = phys_to_virt(pml4e);
    uint64_t pdpte = pdpt[i3]; if (!(pdpte & PTE_P)) return 0;
    if (pdpte & PTE_PS) return (pdpte & ~((1ull<<30)-1)) | (virt & ((1ull<<30)-1));
    uint64_t *pd = phys_to_virt(pdpte);
    uint64_t pde = pd[i2]; if (!(pde & PTE_P)) return 0;
    if (pde & PTE_PS) return (pde & ~((1ull<<21)-1)) | (virt & ((1ull<<21)-1));
    uint64_t *pt = phys_to_virt(pde);
    uint64_t pte = pt[i1]; if (!(pte & PTE_P)) return 0;
    if (pte_out) *pte_out = pte;
    return (pte & ~0xfffull) | (virt & 0xfffull);
}
