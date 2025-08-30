#include "../Strings/strhelper.h"
#include "commands.h"
#include "../Exec/exec.h"

extern void serial_write(const char*);
extern void serial_puthex64(uint64_t);
extern void serial_putc(char);
extern void serial_puti64(int64_t);

#define OUT_MAX 8192

static char outbuf[OUT_MAX];
static size_t outlen;

static inline void out_reset(void){ outlen = 0; outbuf[0] = 0; }
static inline void out_putc(char c){ if (outlen + 1 < OUT_MAX){ outbuf[outlen++] = c; outbuf[outlen] = 0; } }
static inline void out_write(const char *s){ while (*s) out_putc(*s++); }
static inline void out_write_n(const uint8_t *p, uint64_t n){
    for (uint64_t i = 0; i < n && outlen + 1 < OUT_MAX; ++i) out_putc((char)p[i]);
}
static inline void out_hex64(uint64_t x){
    static const char H[] = "0123456789ABCDEF";
    for (int i = 60; i >= 0; i -= 4) out_putc(H[(x >> i) & 0xF]);
}
static inline const char* out_get(void){ return outbuf; }

static void ls_cb_collect(const char *name, vtype_t type){
    out_write(name);
    if (type == V_DIR) out_putc('/');
    out_putc('\n');
}

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

const char* GetResponse(const char* command, const char* argc){
    if (CompareLiteral(command,"cd")){
        if (!argc || !*argc) return "usage: cd path";
        if (vfs_chdir(argc)==0) return "\0";
        return "no such dir";
    }

    if (CompareLiteral(command,"pwd")){
        static char tmp[512];
        vfs_getcwd(tmp, sizeof tmp);
        out_reset(); out_write(tmp);
        return out_get();
    }

    if (CompareLiteral(command,"ls")){
        static char tmp[4096];
        out_reset();
        if (vfs_list_at(vfs_get_cwd(), (argc&&*argc)?argc:".", ls_cb_collect)==0){ return out_get(); }
        return "not found";
    }

    if (CompareLiteral(command,"run")){
        if (!argc || !*argc) return "usage: run /bin/app [arg]\n> ";
        const char *rest = after_first_space_inplace((char*)argc);
        const char *path = first_word_inplace((char*)argc);
        const uint8_t *data; uint64_t sz;
        if (vfs_read_at(vfs_get_cwd(), path, &data, &sz)==0){
            int code=0;
            int rc = exec_run_elf(data, sz, rest, &code);
            out_reset();
            serial_write("exec rc="); serial_puthex64((uint64_t)rc); serial_putc(' ');
            serial_write("exit="); serial_puthex64((uint64_t)code);
            if (rc==0){ out_write("exit "); out_putc('0'+(code%10)); return out_get(); }
            out_write("exec error "); out_hex64((uint64_t)rc); return out_get();
        }
        return "not found";
    }

    if (CompareLiteral(command,"cat")){
        if (!argc||!*argc) return "usage: cat /path";
        const uint8_t *data; uint64_t sz;
        if (vfs_read_at(vfs_get_cwd(), argc, &data, &sz)==0){
            out_reset(); uint64_t max = OUT_MAX-3; if (sz>max) sz=max;
            out_write_n(data, sz); out_putc('\n'); return out_get();
        }
        return "not found";
    }

    if (CompareLiteral(command,"echo"))     return (argc && *argc) ? argc : "\n";
    if (CompareLiteral(command,"shutdown")) return "shutting down\n";
    if (CompareLiteral(command,"help")) return "Help:\n1> echo <arg>\n2> shutdown\n3> ls <path>\n4> cat <path>\n5> pwd <path>\n6> cd <path>\n7> run <path> [arg]\n";
    return "Non-existant command, run help for help\n";
}

const char* ExectuteCommand(const char* command) {
    return GetResponse(first_word_inplace((char*)command), after_first_space_inplace((char*)command));
}