#include "../Strings/strhelper.h"
#include "commands.h"
#include "../Exec/exec.h"
#include "../FS/part.h"
#include "../FS/fat32.h"
#include "../Block/ata_pio.h"
#include "../Memory/kmem.h"
#include "../FS/disk.h"
#include "../FS/fat32.h"
#include "../FS/fat32_path.h"
#include "../FS/paths.h"
#include "../FS/vfs.h"

extern void serial_write(const char*);
extern void serial_puthex64(uint64_t);
extern void serial_putc(char);
extern void serial_puti64(int64_t);

#define OUT_MAX 8192

static char outbuf[OUT_MAX];
static size_t outlen;
static fat32_t gfs;
static char g_cwd[256]="/";
static uint8_t *g_copy_dst;
static uint64_t g_copy_cap;
static uint64_t g_copy_got;

static inline void out_reset(void){ outlen = 0; outbuf[0] = 0; }
static inline void out_putc(char c){ if (outlen + 1 < OUT_MAX){ outbuf[outlen++] = c; outbuf[outlen] = 0; } }
static inline void out_write(const char *s){ while (*s) out_putc(*s++); }
static inline void out_write_n(const uint8_t *p, uint64_t n){
    for (uint64_t i = 0; i < n && outlen + 1 < OUT_MAX; ++i) out_putc((char)p[i]);
}

static inline void b_out_hex(uint64_t x){
    static const char H[]="0123456789ABCDEF";
    for (int i=60;i>=0;i-=4) out_putc(H[(x>>i)&0xF]);
}

static void ls_out(const char* name, uint32_t size, int is_dir){
    out_write(name);
    if (is_dir) out_write("/");
    else { out_write(" "); b_out_hex(size); }
    out_putc('\n');
}
static void ls_cb_vfs(const char* name, vtype_t type){
    ls_out(name, 0, (type==V_DIR));
}

static void set_cwd(const char* p){
    size_t i=0; while (p[i] && i<sizeof g_cwd - 1) g_cwd[i++]=p[i];
    if (i==0) g_cwd[i++]='/';
    g_cwd[i]=0;
}

static int vfs_is_dir(const char* path){
    struct vnode* n = vfs_lookup(path);
    return n && n->type == V_DIR;
}

static int sink_copy_plain(const uint8_t* d, uint32_t n){
    if (g_copy_got + n > g_copy_cap) return -1;
    for (uint32_t i = 0; i < n; i++) g_copy_dst[g_copy_got + i] = d[i];
    g_copy_got += n;
    return 0;
}

static int read_file_into(const char* abs, uint8_t* dst, uint64_t cap, uint64_t* outsz){
    const char* sub = disk_subpath(abs);
    if (sub){
        g_copy_dst = dst; g_copy_cap = cap; g_copy_got = 0;
        if (fat32_read_stream_path(disk_fs(), sub, sink_copy_plain) == 0){
            *outsz = g_copy_got; 
            return 0;
        }
        return -1;
    } else {
        const uint8_t* data; uint64_t sz;
        if (vfs_read(abs, &data, &sz) == 0 && sz <= cap){
            for (uint64_t i = 0; i < sz; i++) dst[i] = data[i];
            *outsz = sz; 
            return 0;
        }
        return -1;
    }
}

static inline const char* out_get(void){ return outbuf; }

static void ls_cb_collect(const char *name, vtype_t type){
    out_write(name);
    if (type == V_DIR) out_putc('/');
    out_putc('\n');
}

static int sink_out(const uint8_t* data, uint32_t len){
    out_write_n(data, len);
    return 0;
}
static void disk_ls_cb(const char* name, uint32_t size, int is_dir){
    out_write(name);
    if (is_dir) out_write("/");
    else { out_write(" "); b_out_hex(size); }
    out_putc('\n');
}

static uint32_t cstrlen(const char* s){
    uint32_t n = 0; while (s && s[n]) n++; return n;
}

static uint8_t elf_slab[524288];

int vfs_list_collect(const char *path, char *dst, size_t cap){
    struct vnode *n = vfs_lookup(path);
    if (!n || n->type != V_DIR) return -1;
    size_t len = 0;
    for (struct vnode *c = n->child; c; c = c->sibling){
        const char *s = c->name;
        while (*s && len + 1 < cap) dst[len++] = *s++;
        if (c->type == V_DIR && len + 1 < cap) dst[len++] = '/';
        if (len + 1 < cap) dst[len++] = '\n';
    }
    if (cap) dst[(len < cap) ? len : cap - 1] = 0;
    return 0;
}

void disk_init_and_mount(void){
    static blockdev_t ata = { ata_read, ata_write, 512 };
    ata_init();
    block_register(&ata);

    part_t p;
    if (mbr_find_fat32(&p)==0){
        if (fat32_mount(&gfs, block_get0(), p.lba_start) == 0)
            set_cwd("/disk");
    }
}

static const char* try_run(const char* name, const char* rest){
    out_reset();
    char abspath[256]; path_resolve(g_cwd, name, abspath, sizeof abspath);
    uint64_t sz=0;
    if (read_file_into(abspath, elf_slab, sizeof elf_slab, &sz)==0){
        int code=0; int rc=exec_run_elf(elf_slab, sz, rest, &code);
        if (rc==0){ out_write("exit "); out_putc('0'+(code%10)); return out_get(); }
        out_write("exec error "); b_out_hex((uint64_t)rc); return out_get();
    }
    char binpath[256]; size_t i=0; const char* pre="/bin/";
    while (pre[i] && i<sizeof(binpath)-1) { binpath[i]=pre[i]; i++; }
    for (size_t j=0; name[j] && i<sizeof(binpath)-1; j++) binpath[i++]=name[j];
    binpath[i]=0;
    if (read_file_into(binpath, elf_slab, sizeof elf_slab, &sz)==0){
        int code=0; int rc=exec_run_elf(elf_slab, sz, rest, &code);
        if (rc==0){ out_write("exit "); out_putc('0'+(code%10)); return out_get(); }
        out_write("exec error "); b_out_hex((uint64_t)rc); return out_get();
    }
    return 0;
}

const char* GetResponse(const char* command, const char* argc){
    if (CompareLiteral(command,"pwd")){ out_reset(); out_write(g_cwd); out_putc('\n'); out_write("> "); return out_get(); }

    if (CompareLiteral(command,"cd")){
        char tmp[256]; const char* p = (argc && *argc)? argc : "/";
        path_resolve(g_cwd,p,tmp,sizeof tmp);
        const char* sub = disk_subpath(tmp);
        if (sub){
            if (!disk_mounted()) return "disk not mounted\n> ";
            if (fat32_is_dir_path(disk_fs(), *sub?sub:"/")){ set_cwd(tmp); return "> "; }
            return "not found\n> ";
        } else {
            if (vfs_chdir(tmp) == 0){ set_cwd(tmp); return "> "; }
            return "not found\n> ";
        }
    }

    if (CompareLiteral(command,"ls")){
        char tmp[256]; const char* p = (argc && *argc)? argc : g_cwd;
        path_resolve(g_cwd,p,tmp,sizeof tmp);
        const char* sub = disk_subpath(tmp);
        out_reset();
        if (sub){
            if (!disk_mounted()){ out_write("disk not mounted\n> "); return out_get(); }
            if (fat32_list_path(disk_fs(), *sub?sub:"/", ls_out)==0){ out_write("> "); return out_get(); }
            out_write("not found\n> "); return out_get();
        } else {
            if (vfs_list_collect(tmp, tmp, 0)==0){ out_write("(override)\n"); }
            if (vfs_list(tmp, ls_cb_vfs) == 0){ out_write("> "); return out_get(); }
            out_write("not found\n> "); return out_get();
        }
    }

    if (CompareLiteral(command,"cat")){
        if (!argc || !*argc) return "usage: cat PATH\n> ";
        out_reset();

        char argbuf[256];
        {
            size_t i=0; while (argc[i] && i+1<sizeof argbuf) { argbuf[i]=argc[i]; i++; }
            while (i && (argbuf[i-1]==' ' || argbuf[i-1]=='\r' || argbuf[i-1]=='\n' || argbuf[i-1]=='\t')) i--;
            argbuf[i]=0;
        }

        if (argbuf[0]=='/' && argbuf[1]=='d' && argbuf[2]=='i' && argbuf[3]=='s' && argbuf[4]=='k'){
            if (!disk_mounted()){ out_write("disk not mounted\n> "); return out_get(); }
            const char* s = argbuf+5; if (*s=='/') s++;
            if (fat32_read_stream_path(disk_fs(), *s? s:"/", sink_out)==0){ out_putc('\n'); out_write("> "); return out_get(); }
            out_write("not found\n> "); return out_get();
        }

        if (g_cwd[0]=='/' && g_cwd[1]=='d' && g_cwd[2]=='i' && g_cwd[3]=='s' && g_cwd[4]=='k' &&
            (argbuf[0] != '/')) {
            if (!disk_mounted()){ out_write("disk not mounted\n> "); return out_get(); }
            if (fat32_read_stream_path(disk_fs(), *argbuf? argbuf:"/", sink_out)==0){ out_putc('\n'); out_write("> "); return out_get(); }
            out_write("not found\n> "); return out_get();
        }

        char abs[256];
        path_resolve(g_cwd, argbuf, abs, sizeof abs);
        const char* sub = disk_subpath(abs);
        if (sub){
            if (!disk_mounted()){ out_write("disk not mounted\n> "); return out_get(); }
            if (fat32_read_stream_path(disk_fs(), *sub? sub:"/", sink_out)==0){ out_putc('\n'); out_write("> "); return out_get(); }
            out_write("not found\n> "); return out_get();
        } else {
            const uint8_t* data; uint64_t sz;
            if (vfs_read(abs,&data,&sz)==0){ out_write_n(data,(uint32_t)sz); out_putc('\n'); out_write("> "); return out_get(); }
            out_write("not found\n> "); return out_get();
        }
    }

    if (CompareLiteral(command,"run")){
        if (!argc||!*argc) return "usage: run PATH [arg]\n> ";
        char tmp[256]; path_resolve(g_cwd,argc,tmp,sizeof tmp);
        const char* rest = after_first_space_inplace((char*)tmp);
        const char* path = first_word_inplace((char*)tmp);
        uint64_t sz=0;
        if (read_file_into(path, elf_slab, sizeof elf_slab, &sz)==0){
            int code=0;
            int rc = exec_run_elf(elf_slab, sz, rest, &code);
            out_reset();
            if (rc==0){ out_write("exit "); out_putc('0'+(code%10)); return out_get(); }
            out_write("exec error "); b_out_hex((uint64_t)rc); return out_get();
        }
        return "not found\n> ";
    }


    if (CompareLiteral(command,"echo"))     return (argc && *argc) ? argc : "\n";
    if (CompareLiteral(command,"shutdown")) return "shutting down\n";
    if (CompareLiteral(command,"help")) return "Help:\n1> echo <arg>\n2> shutdown\n3> ls <path>\n4> cat <path>\n5> pwd <path>\n6> cd <path>\n7> run <path> [arg]\n";
    
    const char* r = try_run(command, argc);
    if (r) return r;

    return "Non-existant command, run help for help\n";
}

const char* ExectuteCommand(const char* command) {
    return GetResponse(first_word_inplace((char*)command), after_first_space_inplace((char*)command));
}