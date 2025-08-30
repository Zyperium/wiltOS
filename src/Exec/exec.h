#pragma once
#include <stdint.h>
#include "app_api.h"

int exec_run_elf(const uint8_t *img, uint64_t len, const char *arg, int *exit_code);
