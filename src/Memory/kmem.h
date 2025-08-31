#pragma once
#include <stdint.h>
#include <stddef.h>

void   kmem_init(void);
void*  kmalloc(size_t n);
void   kfree(void* p);
size_t kmem_used(void);
