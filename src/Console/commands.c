#include "../Strings/strhelper.h"
#include "commands.h"

#define OUT_MAX 8192

static char outbuf[OUT_MAX];
static size_t outlen;

static inline void out_reset(void){ outlen = 0; outbuf[0] = 0; }
static inline void out_putc(char c){ if (outlen + 1 < OUT_MAX){ outbuf[outlen++] = c; outbuf[outlen] = 0; } }
static inline void out_write(const char *s){ while (*s) out_putc(*s++); }
static inline void out_write_n(const uint8_t *p, uint64_t n){
    for (uint64_t i = 0; i < n && outlen + 1 < OUT_MAX; ++i) out_putc((char)p[i]);
}
static inline const char* out_get(void){ return outbuf; }

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
    if (CompareLiteral(command,"ls")){
        static char tmp[4096];
        const char* p = (argc && *argc) ? argc : "/";
        if (vfs_list_collect(p, tmp, sizeof tmp) == 0){
            out_reset(); out_write(tmp); out_write("> ");
            return out_get();
        }
        return "not found\n> ";
    }



    if (CompareLiteral(command,"cat")){
        if (!argc || !*argc) return "usage: cat /path\n> ";
        const uint8_t *data; uint64_t sz;
        if (vfs_read(argc, &data, &sz) == 0){
            out_reset();
            uint64_t max = OUT_MAX - 3;
            if (sz > max) sz = max;
            out_write_n(data, sz);
            out_putc('\n'); out_write("> ");
            return out_get();
        }
        return "not found\n> ";
    }

    if (CompareLiteral(command,"echo"))     return (argc && *argc) ? argc : "";
    if (CompareLiteral(command,"shutdown")) return "shutting down\n";
    return "Non-existant command, run help for help";
}

const char* ExectuteCommand(const char* command) {
    return GetResponse(first_word_inplace((char*)command), after_first_space_inplace((char*)command));
}