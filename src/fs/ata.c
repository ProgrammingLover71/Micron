#include "ata.h"
#include "../io/io.h"

#define ATA_PRIMARY_IO     0x1F0
#define ATA_PRIMARY_CTRL   0x3F6
#define ATA_SECONDARY_IO   0x170
#define ATA_SECONDARY_CTRL 0x376

#define ATA_REG_DATA       0
#define ATA_REG_ERROR      1
#define ATA_REG_SECCOUNT   2
#define ATA_REG_LBA0       3
#define ATA_REG_LBA1       4
#define ATA_REG_LBA2       5
#define ATA_REG_HDDEVSEL   6
#define ATA_REG_COMMAND    7
#define ATA_REG_STATUS     7

#define ATA_CMD_IDENTIFY   0xEC
#define ATA_CMD_READ_PIO   0x20
#define ATA_CMD_WRITE_PIO  0x30
#define ATA_CMD_CACHE_FLUSH 0xE7

#define ATA_SR_ERR         0x01
#define ATA_SR_DRQ         0x08
#define ATA_SR_BSY         0x80

static ata_device_t devices[ATA_MAX_DEVICES];
static int device_count = 0;

static uint16_t channel_io(int channel)
{
    return channel == 0 ? ATA_PRIMARY_IO : ATA_SECONDARY_IO;
}

static uint16_t channel_ctrl(int channel)
{
    return channel == 0 ? ATA_PRIMARY_CTRL : ATA_SECONDARY_CTRL;
}

static void ata_delay(uint16_t ctrl)
{
    (void)inb(ctrl);
    (void)inb(ctrl);
    (void)inb(ctrl);
    (void)inb(ctrl);
}

static int wait_not_bsy(uint16_t io)
{
    for (uint32_t i = 0; i < 1000000; ++i) {
        uint8_t status = inb(io + ATA_REG_STATUS);
        if (status == 0xFF)
            return 0;
        if ((status & ATA_SR_BSY) == 0)
            return 1;
    }
    return 0;
}

static int wait_drq(uint16_t io)
{
    for (uint32_t i = 0; i < 1000000; ++i) {
        uint8_t status = inb(io + ATA_REG_STATUS);
        if (status & ATA_SR_ERR)
            return 0;
        if ((status & ATA_SR_BSY) == 0 && (status & ATA_SR_DRQ))
            return 1;
    }
    return 0;
}

static void copy_model(char *dst, const uint16_t *identify)
{
    for (int i = 0; i < 40; i += 2) {
        uint16_t word = identify[27 + i / 2];
        dst[i] = (char)(word >> 8);
        dst[i + 1] = (char)(word & 0xFF);
    }
    dst[40] = '\0';

    for (int i = 39; i >= 0; --i) {
        if (dst[i] == ' ')
            dst[i] = '\0';
        else
            break;
    }
}

static void probe_drive(int channel, int drive)
{
    if (device_count >= ATA_MAX_DEVICES)
        return;

    uint16_t io = channel_io(channel);
    uint16_t ctrl = channel_ctrl(channel);
    uint16_t identify[256];

    outb(ctrl, 0x02);
    outb(io + ATA_REG_HDDEVSEL, (uint8_t)(0xA0 | (drive << 4)));
    ata_delay(ctrl);

    outb(io + ATA_REG_SECCOUNT, 0);
    outb(io + ATA_REG_LBA0, 0);
    outb(io + ATA_REG_LBA1, 0);
    outb(io + ATA_REG_LBA2, 0);
    outb(io + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    ata_delay(ctrl);

    uint8_t status = inb(io + ATA_REG_STATUS);
    if (status == 0 || status == 0xFF)
        return;

    if (!wait_not_bsy(io))
        return;

    if (inb(io + ATA_REG_LBA1) != 0 || inb(io + ATA_REG_LBA2) != 0)
        return;

    if (!wait_drq(io))
        return;

    insw(io + ATA_REG_DATA, identify, 256);

    ata_device_t *dev = &devices[device_count++];
    dev->present = 1;
    dev->channel = (uint8_t)channel;
    dev->drive = (uint8_t)drive;
    dev->io_base = io;
    dev->ctrl_base = ctrl;
    dev->sectors = ((uint32_t)identify[61] << 16) | identify[60];
    copy_model(dev->model, identify);
}

void ata_init(void)
{
    device_count = 0;
    for (int channel = 0; channel < 2; ++channel) {
        for (int drive = 0; drive < 2; ++drive)
            probe_drive(channel, drive);
    }
}

int ata_device_count(void)
{
    return device_count;
}

const ata_device_t *ata_get_device(int index)
{
    if (index < 0 || index >= device_count)
        return 0;
    return &devices[index];
}

int ata_read_sectors(int index, uint32_t lba, uint8_t count, void *buffer)
{
    if (index < 0 || index >= device_count || count == 0 || !buffer)
        return 0;

    const ata_device_t *dev = &devices[index];
    uint16_t io = dev->io_base;
    uint16_t ctrl = dev->ctrl_base;
    uint8_t *dst = (uint8_t *)buffer;

    if (lba + count > dev->sectors)
        return 0;

    outb(ctrl, 0x02);
    outb(io + ATA_REG_HDDEVSEL,
         (uint8_t)(0xE0 | (dev->drive << 4) | ((lba >> 24) & 0x0F)));
    ata_delay(ctrl);

    outb(io + ATA_REG_SECCOUNT, count);
    outb(io + ATA_REG_LBA0, (uint8_t)(lba & 0xFF));
    outb(io + ATA_REG_LBA1, (uint8_t)((lba >> 8) & 0xFF));
    outb(io + ATA_REG_LBA2, (uint8_t)((lba >> 16) & 0xFF));
    outb(io + ATA_REG_COMMAND, ATA_CMD_READ_PIO);

    for (uint8_t s = 0; s < count; ++s) {
        if (!wait_drq(io))
            return 0;
        insw(io + ATA_REG_DATA, dst + (uint32_t)s * ATA_SECTOR_SIZE, 256);
    }

    return 1;
}

int ata_write_sectors(int index, uint32_t lba, uint8_t count, const void *buffer)
{
    if (index < 0 || index >= device_count || count == 0 || !buffer)
        return 0;

    const ata_device_t *dev = &devices[index];
    uint16_t io = dev->io_base;
    uint16_t ctrl = dev->ctrl_base;
    const uint8_t *src = (const uint8_t *)buffer;

    if (lba + count > dev->sectors)
        return 0;

    outb(ctrl, 0x02);
    outb(io + ATA_REG_HDDEVSEL,
         (uint8_t)(0xE0 | (dev->drive << 4) | ((lba >> 24) & 0x0F)));
    ata_delay(ctrl);

    outb(io + ATA_REG_SECCOUNT, count);
    outb(io + ATA_REG_LBA0, (uint8_t)(lba & 0xFF));
    outb(io + ATA_REG_LBA1, (uint8_t)((lba >> 8) & 0xFF));
    outb(io + ATA_REG_LBA2, (uint8_t)((lba >> 16) & 0xFF));
    outb(io + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);

    for (uint8_t s = 0; s < count; ++s) {
        if (!wait_drq(io))
            return 0;
        outsw(io + ATA_REG_DATA, src + (uint32_t)s * ATA_SECTOR_SIZE, 256);
    }

    outb(io + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
    return wait_not_bsy(io);
}
