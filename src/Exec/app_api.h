#pragma once
#include <stdint.h>

struct app_api {
    void (*puts)(const char*);
    void (*putc)(char);
    int  (*getch)(void);
    void (*hex64)(uint64_t);
    int  (*write_file)(const char*, const uint8_t*, uint64_t);
};
