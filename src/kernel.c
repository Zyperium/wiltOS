#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include "Ports/pmm.h"
#include "Ports/pit.h"
#include "Ports/idt.h"
#include "Ports/pic.h"
#include "Input/kbd.h"
#include "Console/fb.h"
#include "Strings/strhelper.h"
#include "Console/commands.h"
#include "FS/vfs.h"

extern const uint8_t _binary_build_initrd_tar_start[];
extern const uint8_t _binary_build_initrd_tar_end[];
extern void serial_init(void);
extern void serial_write(const char*);
extern void serial_puthex64(uint64_t);
extern void serial_putc(char);
extern void serial_puti64(int64_t);

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request fb_req = {
    .id = LIMINE_FRAMEBUFFER_REQUEST, .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_kernel_address_request kaddr_req = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST, .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_req = {
    .id = LIMINE_MEMMAP_REQUEST, .revision = 0
};

__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER;
__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER;

void *memcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = dst; const uint8_t *s = src;
    for (size_t i = 0; i < n; ++i) d[i] = s[i];
    return dst;
}

void *memset(void *s, int c, size_t n) {
    uint8_t *p = s; for (size_t i = 0; i < n; ++i) p[i] = (uint8_t)c; return s;
}

void *memmove(void *dst, const void *src, size_t n) {
    uint8_t *d = dst; const uint8_t *s = src;
    if (d < s) for (size_t i = 0; i < n; ++i) d[i] = s[i];
    else for (size_t i = n; i; --i) d[i-1] = s[i-1];
    return dst;
}

int memcmp(const void *a, const void *b, size_t n) {
    const uint8_t *x = a, *y = b;
    for (size_t i = 0; i < n; ++i) if (x[i] != y[i]) return x[i] < y[i] ? -1 : 1;
    return 0;
}

static void handle_line(const char* s){
    const char* response = ExectuteCommand(s);
    fb_write(response);
    serial_write(response);
    fb_write("\n> ");

}

void hcf(void) { for (;;) __asm__ __volatile__("hlt"); }

void kmain(void) {
    serial_init();
    if (!LIMINE_BASE_REVISION_SUPPORTED) hcf();
    if (!fb_req.response || fb_req.response->framebuffer_count < 1) hcf();

    struct limine_framebuffer *fb = fb_req.response->framebuffers[0];
    fb_init((void*)fb->address, fb->width, fb->height, fb->pitch, fb->bpp);
    fb_set_colors(0xFFFFFF, 0x000000);
    fb_write("wiltOS: fb console ready\n");

    serial_write("wiltOS: init kernel functions\n");

    uint64_t usable_bytes = 0;
    if (memmap_req.response) {
        struct limine_memmap_response *mm = memmap_req.response;
        serial_write("wiltOS: memmap entries = ");
        serial_puthex64(mm->entry_count);
        serial_write("\n");
        for (size_t i = 0; i < mm->entry_count; ++i) {
            struct limine_memmap_entry *e = mm->entries[i];
            if (e->type == LIMINE_MEMMAP_USABLE) usable_bytes += e->length;
            serial_write("  base="); serial_puthex64(e->base);
            serial_write(" len=");   serial_puthex64(e->length);
            serial_write(" type=");  serial_puthex64(e->type);
            serial_write("\n");
        }
        serial_write("wiltOS: usable RAM bytes = ");
        serial_puthex64(usable_bytes);
        serial_write("\n");
    }

    extern char __kernel_start[], __kernel_end[];
    uint64_t ksize = (uint64_t)(__kernel_end - __kernel_start);
    uint64_t kphys = kaddr_req.response ? kaddr_req.response->physical_base : 0;
    pmm_init(memmap_req.response, kphys, ksize);
    serial_write("wiltOS: kernel phys="); serial_puthex64(kphys);
    serial_write(" size="); serial_puthex64(ksize); serial_write("\n");

    idt_init();
    pic_remap(0x20, 0x28);
    pit_init(100);
    kbd_init();
    pic_setmask(0xFFFC);
    __asm__ __volatile__("sti");

    vfs_init();
    uint64_t rd_len = (uint64_t)(_binary_build_initrd_tar_end - _binary_build_initrd_tar_start);
    int wds = vfs_mount_tar(_binary_build_initrd_tar_start, rd_len);
    serial_write("Value of: ");
    serial_puti64(wds);
    serial_putc('\n');

    char buf[128]; int n=0;
    fb_write("> ");
    for(;;){
        int ch = kbd_getch();
        if (ch == 8 || ch == 127){
            if (n){ n--; fb_putc('\b'); }
            continue;
        }
        if (ch < 0){ __asm__ __volatile__("hlt"); continue; }
        if (ch=='\r' || ch=='\n'){
            fb_putc('\n');
            buf[n]=0; handle_line(buf);
            n=0; continue;
        }
        if (ch==8 || ch==127){
            if (n){ n--; fb_write("\b \b"); }
            continue;
        }
        if (n < (int)sizeof(buf)-1){
            buf[n++]=(char)ch;
            fb_putc((char)ch);
        }
    }
}
