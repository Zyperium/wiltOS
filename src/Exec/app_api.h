#pragma once
#include <stdint.h>

struct app_api {
    void (*putc)(char);
    void (*puts)(const char*);
    int  (*getch)(void);
    int  (*write_file)(const char* abs_path, const uint8_t* data, uint32_t len);
};
