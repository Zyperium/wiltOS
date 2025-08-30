#include <stdint.h>
#include <stddef.h>
#include "../Console/fb.h"
#include "vfs.h"

#define MAX_NODES 512
#define NAMEBUF_SZ 8192

static struct vnode nodes[MAX_NODES];
static uint32_t node_cnt;
static char namebuf[NAMEBUF_SZ];
static uint32_t name_off;
static struct vnode *root;

static char* nb_dup(const char* s, size_t n){
    if (name_off + n + 1 > NAMEBUF_SZ) return 0;
    char *d = &namebuf[name_off];
    for (size_t i=0;i<n;i++) d[i]=s[i];
    d[n]=0; name_off += (uint32_t)(n+1);
    return d;
}
static struct vnode* new_node(const char* name, size_t n, vtype_t t){
    if (node_cnt >= MAX_NODES) return 0;
    struct vnode *v = &nodes[node_cnt++];
    v->type=t; v->name = nb_dup(name,n);
    v->parent=v->sibling=v->child=0; v->data=0; v->size=0;
    return v;
}
static struct vnode* add_child(struct vnode* dir, struct vnode* n){
    n->parent = dir;
    n->sibling = dir->child;
    dir->child = n;
    return n;
}
static struct vnode* find_child(struct vnode* dir, const char* name, size_t n){
    for (struct vnode* c=dir->child; c; c=c->sibling){
        const char *a=c->name; size_t i=0; for(;i<n && a[i] && a[i]==name[i];++i){}
        if (i==n && a[i]==0) return c;
    }
    return 0;
}
static int str_eq(const char* a, const char* b){ while(*a&&*b&&*a==*b){++a;++b;} return *a==0&&*b==0; }

void vfs_init(void){
    node_cnt=0; name_off=0;
    root = new_node("",0,V_DIR);
}

struct vnode* vfs_root(void){ return root; }

static struct vnode* ensure_path(const char* path){
    struct vnode* cur = root;
    const char* p = path;
    while (*p){
        const char* s = p;
        while (*p && *p!='/') ++p;
        size_t len = (size_t)(p - s);
        if (len){
            struct vnode* nxt = find_child(cur, s, len);
            if (!nxt) nxt = add_child(cur, new_node(s,len,V_DIR));
            cur = nxt;
        }
        if (*p=='/') ++p;
    }
    return cur;
}

struct tar_header { char name[100]; char mode[8]; char uid[8]; char gid[8];
 char size[12]; char mtime[12]; char chksum[8]; char typeflag; char linkname[100];
 char magic[6]; char version[2]; char uname[32]; char gname[32];
 char devmajor[8]; char devminor[8]; char prefix[155]; char pad[12]; };

static uint64_t octal(const char *s, size_t n){
    uint64_t v=0; for (size_t i=0;i<n && s[i];++i){ if (s[i]<'0'||s[i]>'7') break; v=(v<<3)+(uint64_t)(s[i]-'0'); } return v;
}

static void split_dir_file(const char* full, size_t *dirlen, const char **base, size_t *baselen){
    const char* last = full; const char* p=full;
    while (*p){ if (*p=='/') last = p+1; ++p; }
    *base = last;
    *baselen = (size_t)(p - last);
    *dirlen = (size_t)(last - full);
}

int vfs_mount_tar(const uint8_t *start, uint64_t len){
    uint32_t count = 0;
    const uint8_t *p = start, *end = start + len;
    while (p + 512 <= end){
        const struct tar_header *h = (const struct tar_header*)p;
        if (h->name[0]==0) break;
        char namebuf_full[256];
        uint32_t off=0;
        if (h->prefix[0]){
            const char* s=h->prefix; while (*s && off<250) namebuf_full[off++]=*s++;
            if (off && namebuf_full[off-1] != '/' && off<250) namebuf_full[off++]='/';
        }
        const char* s=h->name; while (*s && off<255) namebuf_full[off++]=*s++;
        namebuf_full[off]=0;

        uint64_t fsz = octal(h->size, sizeof h->size);
        const uint8_t *data = p + 512;

        size_t dirlen, baselen; const char* base;
        split_dir_file(namebuf_full, &dirlen, &base, &baselen);

        char dironly[256];
        for (size_t i=0;i<dirlen && i<255;i++) dironly[i]=namebuf_full[i];
        dironly[dirlen]=0;
        struct vnode* dir = *dironly ? ensure_path(dironly) : root;

        if (h->typeflag=='5'){
            if (!find_child(dir, base, baselen)) add_child(dir, new_node(base, baselen, V_DIR));
        } else {
            struct vnode* f = find_child(dir, base, baselen);
            if (!f) f = add_child(dir, new_node(base, baselen, V_FILE));
            f->data = data; f->size = fsz;
        }

        uint64_t adv = 512 + ((fsz + 511) & ~511ULL);
        p += adv;
        count++;
    }
    return (int)count;
}

static struct vnode* walk(const char* path){
    if (!path || !*path || *path=='/') return vfs_root();
    struct vnode* cur = vfs_root();
    const char* p = path;
    while (*p){
        const char* s=p; while (*p && *p!='/') ++p;
        size_t n=(size_t)(p-s);
        struct vnode* nxt = find_child(cur, s, n);
        if (!nxt) return 0;
        cur = nxt;
        if (*p=='/') ++p;
    }
    return cur;
}

struct vnode* vfs_lookup(const char *path){ return walk(path && *path=='/'? path+1 : path); }

int vfs_read(const char *path, const uint8_t **data, uint64_t *size){
    struct vnode* n = vfs_lookup(path);
    if (!n || n->type != V_FILE) return -1;
    if (data) *data = n->data;
    if (size) *size = n->size;
    return 0;
}

int vfs_list(const char *path, void (*cb)(const char *name, vtype_t type)){
    struct vnode* n = vfs_lookup(path);
    if (!n || n->type != V_DIR) return -1;
    for (struct vnode* c=n->child; c; c=c->sibling) cb(c->name, c->type);
    return 0;
}
