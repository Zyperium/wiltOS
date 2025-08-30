#include <stdint.h>
#include <stddef.h>
#include "../Console/fb.h"
#include "../Input/kbd.h"
#include "app_api.h"
#include "exec.h"
#include "../FS/vfs.h"

extern void mmu_mark_executable(void* addr, uint64_t len);

typedef struct {
    unsigned char e_ident[16];
    uint16_t e_type, e_machine;
    uint32_t e_version;
    uint64_t e_entry, e_phoff, e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    uint32_t p_type, p_flags;
    uint64_t p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align;
} Elf64_Phdr;

typedef struct {
    uint32_t sh_name, sh_type;
    uint64_t sh_flags, sh_addr, sh_offset, sh_size;
    uint32_t sh_link, sh_info;
    uint64_t sh_addralign, sh_entsize;
} Elf64_Shdr;

typedef struct { uint64_t r_offset, r_info; int64_t r_addend; } Elf64_Rela;

static inline uint32_t R_TYPE(uint64_t r){ return (uint32_t)(r & 0xffffffffu); }
#define PT_LOAD   1
#define PT_DYNAMIC 2
#define SHT_RELA  4
#define R_X86_64_RELATIVE 8
#define DT_NULL       0
#define DT_PLTRELSZ   2
#define DT_RELA       7
#define DT_RELASZ     8
#define DT_RELAENT    9
#define DT_PLTREL    20
#define DT_JMPREL    23

static uint8_t app_area[1024*1024];

static void k_puts(const char* s){ fb_write(s); }
static void k_putc(char c){ fb_putc(c); }
static int  k_getch(void){ return kbd_getch(); }
static void k_hex(uint64_t x){ fb_hex64(x); }
static int k_write_file(const char* path, const uint8_t* data, uint64_t len) { return vfs_write_at(vfs_get_cwd(), path, data, len); }

static int do_rela_block(const uint8_t *img, uint64_t len, uint64_t off, uint64_t sz, uint8_t *base, uint64_t bias){
    if (!off || !sz) return 0;
    if (off >= len || off + sz > len) return -1;
    const Elf64_Rela *ra = (const Elf64_Rela*)(img + off);
    uint64_t cnt = sz / sizeof(Elf64_Rela);
    for (uint64_t i=0;i<cnt;i++){
        if (R_TYPE(ra[i].r_info) != R_X86_64_RELATIVE) return -2;
        uint64_t *loc = (uint64_t*)(bias + ra[i].r_offset);
        *loc = (uint64_t)(base + ra[i].r_addend);
    }
    return 0;
}

static int apply_relocations_dyn(const uint8_t *img, uint64_t len, uint8_t *base, uint64_t bias, const Elf64_Ehdr *eh){
    const Elf64_Phdr *ph = (const Elf64_Phdr*)(img + eh->e_phoff);
    const Elf64_Phdr *dynph = 0;
    for (uint16_t i=0;i<eh->e_phnum;i++) if (ph[i].p_type == PT_DYNAMIC) { dynph = &ph[i]; break; }
    if (!dynph) return 0;
    typedef struct { int64_t d_tag; uint64_t d_un; } Dyn;
    const uint8_t *dstart = img + dynph->p_offset;
    const uint8_t *dend   = dstart + dynph->p_filesz;

    uint64_t rela_off=0, rela_sz=0, rela_ent=0, jmprel_off=0, jmprel_sz=0, pltrel_type=0;

    for (const Dyn *dyn = (const Dyn*)dstart; (const uint8_t*)dyn + sizeof(Dyn) <= dend; ++dyn){
        if (dyn->d_tag == DT_NULL) break;
        if (dyn->d_tag == DT_RELA)     rela_off = dyn->d_un;
        else if (dyn->d_tag == DT_RELASZ)  rela_sz  = dyn->d_un;
        else if (dyn->d_tag == DT_RELAENT) rela_ent = dyn->d_un;
        else if (dyn->d_tag == DT_JMPREL)  jmprel_off = dyn->d_un;
        else if (dyn->d_tag == DT_PLTRELSZ) jmprel_sz = dyn->d_un;
        else if (dyn->d_tag == DT_PLTREL)   pltrel_type = dyn->d_un;
    }

    int rc;
    rc = do_rela_block(img, len, rela_off,   rela_sz,   base, bias); if (rc) return rc;
    if (pltrel_type == DT_RELA || pltrel_type == 0)
        rc = do_rela_block(img, len, jmprel_off, jmprel_sz, base, bias);
    else
        rc = -3;
    return rc;
}

typedef int (*entry_t)(const struct app_api*, const char*);

static int call_entry_aligned(entry_t entry, const struct app_api* api, const char* arg){
    int ret;
    __asm__ __volatile__(
        "mov %%rsp, %%r11\n\t"
        "and $~0xF, %%rsp\n\t"
        "sub $8, %%rsp\n\t"
        "call *%3\n\t"
        "mov %%r11, %%rsp\n\t"
        : "=a"(ret)
        : "D"(api), "S"(arg), "r"(entry)
        : "rcx","r11","r8","r9","r10","r12","r13","r14","r15","memory"
    );
    return ret;
}

int exec_run_elf(const uint8_t *img, uint64_t len, const char *arg, int *exit_code){
    if (len < sizeof(Elf64_Ehdr)) return -1;
    const Elf64_Ehdr *eh = (const Elf64_Ehdr*)img;
    if (!(eh->e_ident[0]==0x7F && eh->e_ident[1]=='E' && eh->e_ident[2]=='L' && eh->e_ident[3]=='F')) return -2;
    if (eh->e_machine != 62) return -3;

    const Elf64_Phdr *ph = (const Elf64_Phdr*)(img + eh->e_phoff);
    uint64_t vmin = ~0ull, vmax = 0;
    for (uint16_t i=0;i<eh->e_phnum;i++){
        if (ph[i].p_type != PT_LOAD) continue;
        if (!ph[i].p_memsz) continue;
        if (ph[i].p_vaddr < vmin) vmin = ph[i].p_vaddr;
        uint64_t end = ph[i].p_vaddr + ph[i].p_memsz;
        if (end > vmax) vmax = end;
    }
    if (vmin == ~0ull) return -4;

    uint64_t total = (vmax - vmin + 0xFFF) & ~0xFFFull;
    uint8_t *base = (uint8_t*)(((uintptr_t)app_area + 0xFFF) & ~0xFFFul);
    mmu_mark_executable(base, total);
    if ((uint64_t)(base - app_area) + total > sizeof(app_area)) return -5;

    for (uint64_t i=0;i<total;i++) base[i]=0;

    for (uint16_t i=0;i<eh->e_phnum;i++){
        if (ph[i].p_type != PT_LOAD) continue;
        uint64_t off = ph[i].p_offset;
        uint64_t dst = ph[i].p_vaddr - vmin;
        uint64_t fs  = ph[i].p_filesz;
        if (off + fs > len) return -6;
        for (uint64_t b=0;b<fs;b++) base[dst + b] = img[off + b];
    }

    uint64_t bias = (uint64_t)base - vmin;
    int rc = apply_relocations_dyn(img, len, base, bias, eh);
    if (rc) return -7;

    struct app_api api = { k_puts, k_putc, k_getch, k_hex, k_write_file };
    uint64_t entry_off = eh->e_entry - vmin;
    if (entry_off >= total) return -8;

    __asm__ __volatile__("sti");
    int ec = call_entry_aligned((entry_t)(base + entry_off), &api, arg);
    if (exit_code) *exit_code = ec;
    return 0;
}
