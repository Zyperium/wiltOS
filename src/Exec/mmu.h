#pragma once
#include <stdint.h>
void mmu_mark_executable(void* addr, uint64_t len);
