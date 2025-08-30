#pragma once
#include <stdint.h>

void fb_init(void *addr, uint32_t width, uint32_t height, uint32_t pitch, uint16_t bpp);
void fb_set_colors(uint32_t fg, uint32_t bg);
void fb_clear(void);
void fb_putc(char c);
void fb_write(const char *s);
void fb_hex64(uint64_t x);
void fb_set_cursor(uint32_t cx, uint32_t cy);
void fb_get_cell_metrics(uint32_t *cw, uint32_t *ch, uint32_t *cols, uint32_t *rows);
