#pragma once
#include <stddef.h>

void path_resolve(const char* cwd, const char* in, char* out, size_t cap);
const char* disk_subpath(const char* abs);
