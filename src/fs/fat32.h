#pragma once

#include <stdint.h>

#define FAT32_NAME_MAX 13

typedef struct {
    char name[FAT32_NAME_MAX];
    uint8_t attr;
    uint32_t first_cluster;
    uint32_t size;
} fat32_dir_entry_t;

int fat32_init(void);
int fat32_is_mounted(void);
const char *fat32_mount_name(void);
int fat32_list_root(fat32_dir_entry_t *entries, int max_entries);
int fat32_read_file(const char *name, uint8_t *buffer, uint32_t buffer_size, uint32_t *bytes_read);
int fat32_create_file(const char *name);
int fat32_delete_file(const char *name);
int fat32_write_existing_file(const char *name, const uint8_t *buffer, uint32_t size);
