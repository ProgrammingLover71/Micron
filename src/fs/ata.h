#pragma once

#include <stdint.h>

#define ATA_SECTOR_SIZE 512
#define ATA_MAX_DEVICES 4

typedef struct {
    uint8_t present;
    uint8_t channel;
    uint8_t drive;
    uint16_t io_base;
    uint16_t ctrl_base;
    uint32_t sectors;
    char model[41];
} ata_device_t;

void ata_init(void);
int ata_device_count(void);
const ata_device_t *ata_get_device(int index);
int ata_read_sectors(int index, uint32_t lba, uint8_t count, void *buffer);
int ata_write_sectors(int index, uint32_t lba, uint8_t count, const void *buffer);
