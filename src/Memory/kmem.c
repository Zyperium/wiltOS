#include <stdint.h>
#include <stddef.h>
#include "kmem.h"
#include "../MM/vmm.h"
#include "../MM/pmm.h"

#define KHEAP_BASE 0xFFFFA00000000000ull
#define KHEAP_MAX  (64ull*1024*1024)
#define ALIGN16(x) (((x)+15ull)&~15ull)

typedef struct blk { uint64_t size; struct blk* next; } blk_t;

static uint8_t *heap_lo = (uint8_t*)KHEAP_BASE;
static uint8_t *heap_hi = (uint8_t*)KHEAP_BASE;
static uint8_t *heap_cap = (uint8_t*)(KHEAP_BASE + KHEAP_MAX);
static blk_t *free_list;
static uint64_t used_bytes;

static int grow(size_t need){
    size_t cur = (size_t)(heap_cap - heap_hi);
    if (cur >= need) return 0;
    size_t add = ALIGN16(need - cur);
    size_t pages = (add + 0xfff) >> 12;
    for (size_t i=0;i<pages;i++){
        uint64_t pa = pmm_alloc();
        if (!pa) return -1;
        vmm_map(pa, (uint64_t)heap_hi, 0x1000, VMM_RW | VMM_NX);
        heap_hi += 0x1000;
    }
    return 0;
}

void kmem_init(void){
    free_list = 0;
    used_bytes = 0;
    grow(0x4000);
}

static void list_insert(blk_t* b){
    if (!free_list || (uint8_t*)b < (uint8_t*)free_list){
        b->next = free_list; free_list = b; return;
    }
    blk_t* p = free_list;
    while (p->next && (uint8_t*)p->next < (uint8_t*)b) p = p->next;
    b->next = p->next; p->next = b;
}

static void list_coalesce(void){
    blk_t* p = free_list;
    while (p && p->next){
        uint8_t* end = (uint8_t*)p + sizeof(blk_t) + p->size;
        if (end == (uint8_t*)p->next){
            p->size += sizeof(blk_t) + p->next->size;
            p->next = p->next->next;
        } else p = p->next;
    }
}

void* kmalloc(size_t n){
    if (!n) return 0;
    n = ALIGN16(n);
    blk_t **pp = &free_list, *p = free_list;
    while (p){
        if (p->size >= n){
            size_t rem = p->size - n;
            if (rem >= sizeof(blk_t) + 16){
                uint8_t* base = (uint8_t*)p;
                blk_t* nxt = (blk_t*)(base + sizeof(blk_t) + n);
                nxt->size = rem - sizeof(blk_t);
                nxt->next = p->next;
                *pp = nxt;
            } else {
                n = p->size;
                *pp = p->next;
            }
            used_bytes += n;
            return (uint8_t*)p + sizeof(blk_t);
        }
        pp = &p->next; p = p->next;
    }
    size_t need = sizeof(blk_t) + n;
    if (grow(need)) return 0;
    blk_t* b = (blk_t*)heap_lo;
    b->size = (uint64_t)(heap_hi - heap_lo - sizeof(blk_t));
    b->next = 0;
    free_list = b;
    heap_lo = heap_hi;
    void* r = kmalloc(n);
    return r;
}

void kfree(void* p){
    if (!p) return;
    blk_t* b = (blk_t*)((uint8_t*)p - sizeof(blk_t));
    used_bytes -= b->size;
    list_insert(b);
    list_coalesce();
}

size_t kmem_used(void){ return (size_t)used_bytes; }
