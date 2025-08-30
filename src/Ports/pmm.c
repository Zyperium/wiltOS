#include <stdint.h>
#include <stddef.h>
#include <limine.h>

#define PAGE_SIZE 4096ULL
#define PMM_MAX_PHYS 0x100000000ULL
#define PMM_MAX_PAGES (PMM_MAX_PHYS / PAGE_SIZE)

static uint8_t bitmap[PMM_MAX_PAGES / 8];
static uint64_t pages_total = 0;
static uint64_t pages_used = 0;
static uint64_t pages_limit = 0;

static inline void bm_set(uint64_t i){ bitmap[i>>3] |=  (uint8_t)(1u << (i&7)); }
static inline void bm_clr(uint64_t i){ bitmap[i>>3] &= (uint8_t)~(1u << (i&7)); }
static inline int  bm_tst(uint64_t i){ return (bitmap[i>>3] >> (i&7)) & 1u; }

static inline uint64_t align_up(uint64_t x, uint64_t a){ return (x + a - 1) & ~(a - 1); }
static inline uint64_t align_dn(uint64_t x, uint64_t a){ return x & ~(a - 1); }

static void mark_used(uint64_t base, uint64_t len){
    uint64_t s = align_dn(base, PAGE_SIZE) / PAGE_SIZE;
    uint64_t e = align_up(base + len, PAGE_SIZE) / PAGE_SIZE;
    if (e > pages_limit) e = pages_limit;
    for (uint64_t i = s; i < e; ++i) if (!bm_tst(i)) { bm_set(i); ++pages_used; }
}

static void mark_free(uint64_t base, uint64_t len){
    uint64_t s = align_up(base, PAGE_SIZE) / PAGE_SIZE;
    uint64_t e = align_dn(base + len, PAGE_SIZE) / PAGE_SIZE;
    if (e > pages_limit) e = pages_limit;
    for (uint64_t i = s; i < e; ++i) if (bm_tst(i)) { bm_clr(i); --pages_used; }
}

void pmm_init(struct limine_memmap_response *mm, uint64_t kphys, uint64_t ksize){
    for (uint64_t i = 0; i < sizeof bitmap; ++i) bitmap[i] = 0xFF;
    pages_limit = PMM_MAX_PAGES;
    pages_total = 0;
    pages_used  = pages_limit;

    if (!mm) return;

    uint64_t max_phys = 0;
    for (uint64_t i = 0; i < mm->entry_count; ++i){
        struct limine_memmap_entry *e = mm->entries[i];
        uint64_t end = e->base + e->length;
        if (end > max_phys) max_phys = end;
    }
    if (max_phys < PMM_MAX_PHYS) pages_limit = align_up(max_phys, PAGE_SIZE) / PAGE_SIZE;
    pages_total = pages_limit;

    for (uint64_t i = 0; i < mm->entry_count; ++i){
        struct limine_memmap_entry *e = mm->entries[i];
        if (e->type == LIMINE_MEMMAP_USABLE) mark_free(e->base, e->length);
    }

    if (ksize) mark_used(kphys, ksize);
    mark_used(0, 0x100000);
}

uint64_t pmm_alloc(void){
    for (uint64_t i = 0; i < pages_limit; ++i){
        if (!bm_tst(i)){
            bm_set(i);
            ++pages_used;
            return i * PAGE_SIZE;
        }
    }
    return 0;
}

void pmm_free(uint64_t paddr){
    uint64_t i = paddr / PAGE_SIZE;
    if (i >= pages_limit) return;
    if (bm_tst(i)){
        bm_clr(i);
        --pages_used;
    }
}

uint64_t pmm_total(void){ return pages_total * PAGE_SIZE; }
uint64_t pmm_used(void){ return pages_used * PAGE_SIZE; }
