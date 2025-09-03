#include <stdint.h>
#include <stddef.h>
#include "../Console/fb.h"
#include "../Input/kbd.h"
#include "../MM/pmm.h"
#include "../MM/vmm.h"
#include "app_api.h"
#include "exec.h"
#include "../FS/vfs.h"
#include "../FS/disk.h"
#include "../FS/fat32_path.h"
#include "../FS/paths.h"

extern void serial_write(const char*);
extern void serial_puthex64(uint64_t);

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

typedef struct { uint64_t r_offset, r_info; int64_t r_addend; } Elf64_Rela;

static inline uint32_t R_TYPE(uint64_t r){ return (uint32_t)(r & 0xffffffffu); }
#define PT_LOAD     1
#define PT_DYNAMIC  2
#define PT_INTERP   3
#define PT_TLS      7
#define SHT_RELA    4
#define ET_DYN      3
#define EM_X86_64   62
#define DT_NULL     0
#define DT_NEEDED   1
#define DT_PLTRELSZ 2
#define DT_RELA     7
#define DT_RELASZ   8
#define DT_RELAENT  9
#define DT_PLTREL   20
#define DT_JMPREL   23
#define R_X86_64_RELATIVE 8
#define PF_X 1u
#define PF_W 2u
#define PF_R 4u

#define APP_BASE     0xFFFFC00000000000ull
#define APP_MAX_SIZE (16ull*1024*1024)

static void k_puts(const char* s){ fb_write(s); }
static void k_putc(char c){ fb_putc(c); }
static int  k_getch(void){ return kbd_getch(); }
static void k_hex(uint64_t x){ fb_hex64(x); }
static int k_write_file(const char* path, const uint8_t* data, uint64_t len){
#ifdef HAVE_VFS_GET_CWD
    return vfs_write_at(vfs_get_cwd(), path, data, len);
#else
    return vfs_write(path, data, len);
#endif
}

static void str_copy(char *d, const char *s, size_t cap){ size_t i=0; while(s && s[i] && i+1<cap){ d[i]=s[i]; i++; } d[i]=0; }

static int do_rela_block(const uint8_t *img, uint64_t len, uint64_t off, uint64_t sz, uint64_t bias){
    if (!off || !sz) return 0;
    if (off >= len || sz > len - off) return -1;
    const Elf64_Rela *ra = (const Elf64_Rela*)(img + off);
    uint64_t cnt = sz / sizeof(Elf64_Rela);
    for (uint64_t i=0;i<cnt;i++){
        if (R_TYPE(ra[i].r_info) != R_X86_64_RELATIVE) return -2;
        *(uint64_t*)(bias + ra[i].r_offset) = bias + (uint64_t)ra[i].r_addend;
    }
    return 0;
}

static int scan_dynamic(const uint8_t *img, uint64_t len, const Elf64_Ehdr *eh,
                        int *needed_cnt_out, uint64_t *jmprel_sz_out){
    if (eh->e_phoff >= len || !eh->e_phnum) return -1;
    const Elf64_Phdr *ph = (const Elf64_Phdr*)(img + eh->e_phoff);
    const Elf64_Phdr *dynph = 0;
    for (uint16_t i=0;i<eh->e_phnum;i++) if (ph[i].p_type == PT_DYNAMIC){ dynph = &ph[i]; break; }
    if (!dynph){ if (needed_cnt_out) *needed_cnt_out = 0; if (jmprel_sz_out) *jmprel_sz_out = 0; return 0; }
    if (dynph->p_offset >= len || dynph->p_filesz > len - dynph->p_offset) return -1;

    typedef struct { int64_t d_tag; uint64_t d_un; } Dyn;
    const uint8_t *dstart = img + dynph->p_offset;
    const uint8_t *dend   = dstart + dynph->p_filesz;

    int needed = 0; uint64_t jmprel_sz = 0;
    for (const Dyn *d=(const Dyn*)dstart; (const uint8_t*)d + sizeof(Dyn) <= dend; ++d){
        if (d->d_tag == DT_NULL) break;
        if (d->d_tag == DT_NEEDED)   needed++;
        else if (d->d_tag == DT_PLTRELSZ) jmprel_sz = d->d_un;
    }
    if (needed_cnt_out)   *needed_cnt_out = needed;
    if (jmprel_sz_out)    *jmprel_sz_out = jmprel_sz;
    return 0;
}

static int apply_relocations_dyn(const uint8_t *img, uint64_t len, const Elf64_Ehdr *eh, uint64_t bias){
    if (eh->e_phoff >= len || eh->e_phnum == 0) return -1;
    if ((uint64_t)eh->e_phentsize * eh->e_phnum > len - eh->e_phoff) return -1;
    const Elf64_Phdr *ph = (const Elf64_Phdr*)(img + eh->e_phoff);
    const Elf64_Phdr *dynph = 0;
    for (uint16_t i=0;i<eh->e_phnum;i++) if (ph[i].p_type == PT_DYNAMIC) { dynph = &ph[i]; break; }
    if (!dynph) return 0;
    if (dynph->p_offset >= len || dynph->p_filesz > len - dynph->p_offset) return -1;

    typedef struct { int64_t d_tag; uint64_t d_un; } Dyn;
    const uint8_t *dstart = img + dynph->p_offset;
    const uint8_t *dend   = dstart + dynph->p_filesz;

    uint64_t rela_off=0, rela_sz=0, jmprel_off=0, jmprel_sz=0, pltrel_type=0;

    for (const Dyn *d=(const Dyn*)dstart; (const uint8_t*)d + sizeof(Dyn) <= dend; ++d){
        if (d->d_tag == DT_NULL) break;
        if (d->d_tag == DT_RELA)       rela_off = d->d_un;
        else if (d->d_tag == DT_RELASZ)   rela_sz  = d->d_un;
        else if (d->d_tag == DT_JMPREL)   jmprel_off = d->d_un;
        else if (d->d_tag == DT_PLTRELSZ) jmprel_sz  = d->d_un;
        else if (d->d_tag == DT_PLTREL)   pltrel_type = d->d_un;
    }

    int rc = do_rela_block(img, len, rela_off,   rela_sz,   bias);
    if (rc) return rc;
    if (pltrel_type == DT_RELA || pltrel_type == 0)
        rc = do_rela_block(img, len, jmprel_off, jmprel_sz, bias);
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

static int starts_with(const char* s, const char* p){ while(*p){ if(*s++!=*p++) return 0; } return 1; }
static int has_slash(const char* s){ for(;*s;s++) if(*s=='/') return 1; return 0; }
static int ends_with_ci(const char* s, const char* suf){
    size_t ls=0, le=0; while(s[ls]) ls++; while(suf[le]) le++; if(le>ls) return 0;
    for(size_t i=0;i<le;i++){ char a=s[ls-le+i], b=suf[i]; if(a>='a'&&a<='z') a-=32; if(b>='a'&&b<='z') b-=32; if(a!=b) return 0; }
    return 1;
}

static int api_write_file_impl(const char* path, const uint8_t* data, uint32_t len){
    if (!path || !*path || !data) return -1;
    if (!disk_mounted()) return -2;

    char abs[256];
    if (path[0]=='/') str_copy(abs, path, sizeof abs);
    else { str_copy(abs, "/disk/", sizeof abs); size_t k=0; while(abs[k]) k++; str_copy(abs+k, path, sizeof abs - k); }

    if (!starts_with(abs,"/disk")){ char t[256]; str_copy(t,"/disk",sizeof t); size_t k=0; while(t[k]) k++; if (abs[0]!='/') t[k++]='/'; str_copy(t+k,abs,sizeof t - k); str_copy(abs,t,sizeof abs); }

    const char* s = abs + 5; if (*s=='/') s++;

    if (!has_slash(s) && ends_with_ci(s,".ELF")){
        char binp[256]; str_copy(binp,"/disk/BIN/",sizeof binp); size_t k=0; while(binp[k]) k++; str_copy(binp+k,s,sizeof binp - k);
        const char* sub = disk_subpath(binp); if (!sub) return -3;
        return fat32_write_file_path(disk_fs(), sub, data, len);
    }

    const char* sub = disk_subpath(abs); if (!sub) return -3;
    return fat32_write_file_path(disk_fs(), sub, data, len);
}

static int map_segment(uint64_t dst, uint64_t size, uint32_t pflags){
    uint64_t start = dst & ~0xFFFull;
    uint64_t end   = (dst + size + 0xFFF) & ~0xFFFull;
    uint64_t flags = VMM_RW;                 /* force W while loading */
    if (!(pflags & PF_X)) flags |= VMM_NX;   /* still enforce NX for non-exec */
    flags |= VMM_G;
    for (uint64_t va = start; va < end; va += 0x1000){
        uint64_t pa = pmm_alloc(); if (!pa) return -1;
        int rc = vmm_map(pa, va, 0x1000, flags);
        if (rc) return rc;
    }
    return 0;
}

static void unmap_range(uint64_t base, uint64_t total){
    if (!total) return;
    vmm_unmap(base, total, 1);
}

int exec_run_elf(const uint8_t *img, uint64_t len, const char *arg, int *exit_code){
    if (len < sizeof(Elf64_Ehdr)) return -1;
    const Elf64_Ehdr *eh = (const Elf64_Ehdr*)img;
    if (!(eh->e_ident[0]==0x7F && eh->e_ident[1]=='E' && eh->e_ident[2]=='L' && eh->e_ident[3]=='F')) return -2;
    if (eh->e_ident[4] != 2 || eh->e_ident[5] != 1) return -2;
    if (eh->e_machine != EM_X86_64) return -3;
    if (eh->e_type != ET_DYN) return -4;
    if (eh->e_phoff >= len) return -5;
    if (!eh->e_phnum || eh->e_phentsize != sizeof(Elf64_Phdr)) return -5;
    if ((uint64_t)eh->e_phnum * sizeof(Elf64_Phdr) > len - eh->e_phoff) return -5;

    const Elf64_Phdr *ph = (const Elf64_Phdr*)(img + eh->e_phoff);
    uint64_t vmin = ~0ull, vmax = 0;
    int has_interp = 0, has_tls = 0;

    for (uint16_t i=0;i<eh->e_phnum;i++){
        if (ph[i].p_type == PT_INTERP && ph[i].p_filesz) has_interp = 1;
        if (ph[i].p_type == PT_TLS && ph[i].p_memsz) has_tls = 1;
        if (ph[i].p_type != PT_LOAD) continue;
        if (!ph[i].p_memsz) continue;
        if (ph[i].p_vaddr < vmin) vmin = ph[i].p_vaddr;
        uint64_t end = ph[i].p_vaddr + ph[i].p_memsz;
        if (end > vmax) vmax = end;
    }
    serial_write("ph: flags interp="); serial_puthex64(has_interp);
    serial_write(" tls=");             serial_puthex64(has_tls);
    serial_write("\n");
    if (has_tls) return -6;
    if (vmin == ~0ull) return -7;

    int needed_cnt = 0; uint64_t jmprel_sz = 0;
    if (scan_dynamic(img, len, eh, &needed_cnt, &jmprel_sz) < 0) return -5;
    if (needed_cnt > 0 || jmprel_sz > 0) return -6;

    uint64_t total = (vmax - vmin + 0xFFF) & ~0xFFFull;
    if (total > APP_MAX_SIZE) return -8;
    uint64_t base = APP_BASE;
    uint64_t bias = base - vmin;

    for (uint16_t i=0;i<eh->e_phnum;i++){
        if (ph[i].p_type != PT_LOAD) continue;
        if (ph[i].p_memsz == 0) continue;
        uint64_t dst = bias + ph[i].p_vaddr;
        if (map_segment(dst, ph[i].p_memsz, ph[i].p_flags)){ unmap_range(base, total); return -9; }
        if (ph[i].p_offset >= len) { unmap_range(base, total); return -10; }
        if (ph[i].p_filesz > ph[i].p_memsz) { unmap_range(base, total); return -10; }
        if (ph[i].p_filesz && ph[i].p_offset + ph[i].p_filesz > len){ unmap_range(base, total); return -10; }

        uint8_t *d = (uint8_t*)dst;
        for (uint64_t b=0;b<ph[i].p_filesz;b++) d[b] = img[ph[i].p_offset + b];
        for (uint64_t b=ph[i].p_filesz;b<ph[i].p_memsz;b++) d[b] = 0;
    }

    int rc = apply_relocations_dyn(img, len, eh, bias);
    if (rc){ unmap_range(base, total); return -11; }

    for (uint16_t i=0;i<eh->e_phnum;i++){
    const Elf64_Phdr *p = &ph[i];
    if (p->p_type != PT_LOAD || p->p_memsz == 0) continue;
        if (!(p->p_flags & PF_W)) {
            uint64_t dst = bias + p->p_vaddr;
            vmm_set_writable(dst, p->p_memsz, 0);
        }
    }

    struct app_api api = { k_putc, k_puts, k_getch, api_write_file_impl };
    uint64_t entry = bias + eh->e_entry;
    if (entry < base || entry >= base + total){ unmap_range(base, total); return -12; }

    __asm__ __volatile__("sti");
    int ec = call_entry_aligned((entry_t)entry, &api, arg);
    if (exit_code) *exit_code = ec;

    unmap_range(base, total);
    return 0;
}