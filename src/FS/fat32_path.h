#pragma once
#include <stdint.h>
#include "fat32.h"

int fat32_is_dir_path(fat32_t* fs, const char* path);
int fat32_list_path(fat32_t* fs, const char* path, void (*cb)(const char* name, uint32_t size, int is_dir));
int fat32_read_stream_path(fat32_t* fs, const char* path, int (*sink)(const uint8_t* data, uint32_t len));
int fat32_write_file_path(fat32_t* fs, const char* path, const uint8_t* data, uint32_t sz);
