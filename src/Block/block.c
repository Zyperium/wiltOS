#include <stdint.h>
#include "block.h"
static blockdev_t* g0;
int block_register(blockdev_t* d){ g0=d; return 0; }
blockdev_t* block_get0(void){ return g0; }
