#pragma once
#include <stdint.h>
typedef enum { V_DIR=1, V_FILE=2 } vtype_t;

struct vnode {
    vtype_t type;
    const char *name;
    struct vnode *parent, *sibling, *child;
    const uint8_t *data;
    uint64_t size;
};

void vfs_init(void);
struct vnode* vfs_root(void);

struct vnode* vfs_get_cwd(void);
void vfs_set_cwd(struct vnode* d);
int vfs_chdir(const char *path);

struct vnode* vfs_lookup(const char *path);
struct vnode* vfs_lookup_at(struct vnode* base, const char *path);

int vfs_list(const char *path, void (*cb)(const char *name, vtype_t type));
int vfs_list_at(struct vnode* base, const char *path, void (*cb)(const char *name, vtype_t type));

int vfs_read(const char *path, const uint8_t **data, uint64_t *size);
int vfs_read_at(struct vnode* base, const char *path, const uint8_t **data, uint64_t *size);

size_t vfs_getcwd(char *dst, size_t cap);
int vfs_mount_tar(const uint8_t*, uint64_t);

int vfs_write(const char *path, const uint8_t *data, uint64_t size);
int vfs_write_at(struct vnode* base, const char *path, const uint8_t *data, uint64_t size);