#include "fat32.h"
#include "ata.h"

typedef struct __attribute__((packed)) {
    uint8_t jump[3];
    char oem[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fats;
    uint16_t root_entries;
    uint16_t total_sectors_16;
    uint8_t media;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fsinfo_sector;
    uint16_t backup_boot_sector;
    uint8_t reserved[12];
    uint8_t drive_number;
    uint8_t reserved1;
    uint8_t boot_signature;
    uint32_t volume_id;
    char volume_label[11];
    char fs_type[8];
} fat32_bpb_t;

typedef struct __attribute__((packed)) {
    char name[11];
    uint8_t attr;
    uint8_t nt_reserved;
    uint8_t create_time_tenth;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t access_date;
    uint16_t first_cluster_high;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t first_cluster_low;
    uint32_t file_size;
} fat32_raw_entry_t;

static int mounted = 0;
static int disk_index = -1;
static uint32_t partition_lba = 0;
static uint32_t fat_lba = 0;
static uint32_t data_lba = 0;
static uint32_t root_cluster = 2;
static uint32_t sectors_per_fat = 0;
static uint8_t sectors_per_cluster = 0;
static char mount_name[32];

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int mem_eq(const char *a, const char *b, int len)
{
    for (int i = 0; i < len; ++i) {
        if (a[i] != b[i])
            return 0;
    }
    return 1;
}

static void str_copy(char *dst, const char *src, int max)
{
    int i = 0;
    if (max <= 0)
        return;
    while (src[i] && i < max - 1) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

static uint32_t cluster_to_lba(uint32_t cluster)
{
    return data_lba + (cluster - 2) * sectors_per_cluster;
}

static int read_sector(uint32_t lba, uint8_t *buffer)
{
    return ata_read_sectors(disk_index, lba, 1, buffer);
}

static int write_sector(uint32_t lba, const uint8_t *buffer)
{
    return ata_write_sectors(disk_index, lba, 1, buffer);
}

static int read_fat_entry(uint32_t cluster, uint32_t *next)
{
    uint8_t sector[ATA_SECTOR_SIZE];
    uint32_t fat_offset = cluster * 4;
    uint32_t sector_lba = fat_lba + fat_offset / ATA_SECTOR_SIZE;
    uint32_t offset = fat_offset % ATA_SECTOR_SIZE;

    if (!read_sector(sector_lba, sector))
        return 0;

    *next = rd32(&sector[offset]) & 0x0FFFFFFF;
    return 1;
}

static int is_eoc(uint32_t cluster)
{
    return cluster >= 0x0FFFFFF8;
}

static int write_fat_entry(uint32_t cluster, uint32_t value)
{
    uint8_t sector[ATA_SECTOR_SIZE];
    uint32_t fat_offset = cluster * 4;
    uint32_t sector_lba = fat_lba + fat_offset / ATA_SECTOR_SIZE;
    uint32_t offset = fat_offset % ATA_SECTOR_SIZE;
    uint32_t existing, new_value;

    if (!read_sector(sector_lba, sector))
        return 0;

    existing = rd32(&sector[offset]);
    new_value = (value & 0x0FFFFFFF) | (existing & 0xF0000000);

    sector[offset]     = (uint8_t)(new_value & 0xFF);
    sector[offset + 1] = (uint8_t)((new_value >> 8) & 0xFF);
    sector[offset + 2] = (uint8_t)((new_value >> 16) & 0xFF);
    sector[offset + 3] = (uint8_t)((new_value >> 24) & 0xFF);

    return write_sector(sector_lba, sector);
}

static void format_name(const fat32_raw_entry_t *raw, char *out)
{
    int pos = 0;
    for (int i = 0; i < 8 && raw->name[i] != ' '; ++i)
        out[pos++] = raw->name[i];

    if (raw->name[8] != ' ') {
        out[pos++] = '.';
        for (int i = 8; i < 11 && raw->name[i] != ' '; ++i)
            out[pos++] = raw->name[i];
    }
    out[pos] = '\0';
}

static char upcase(char c)
{
    if (c >= 'a' && c <= 'z')
        return (char)(c - 32);
    return c;
}

static int name_matches(const char *query, const char *entry)
{
    int i = 0;
    while (query[i] && entry[i]) {
        if (upcase(query[i]) != upcase(entry[i]))
            return 0;
        ++i;
    }
    return query[i] == '\0' && entry[i] == '\0';
}

static void make_83_name(const char *name, char out[11])
{
    int i = 0;
    int pos = 0;

    for (int j = 0; j < 11; ++j)
        out[j] = ' ';

    while (name[i] && name[i] != '.' && pos < 8) {
        out[pos++] = upcase(name[i]);
        ++i;
    }

    if (name[i] == '.')
        ++i;

    pos = 8;
    while (name[i] && pos < 11) {
        out[pos++] = upcase(name[i]);
        ++i;
    }
}

static int usable_entry(const fat32_raw_entry_t *raw)
{
    uint8_t first = (uint8_t)raw->name[0];
    if (first == 0x00 || first == 0xE5)
        return 0;
    if ((raw->attr & 0x0F) == 0x0F)
        return 0;
    if (raw->attr & 0x08)
        return 0;
    return 1;
}

static int mount_at(int dev, uint32_t lba)
{
    uint8_t sector[ATA_SECTOR_SIZE];
    if (!ata_read_sectors(dev, lba, 1, sector))
        return 0;

    fat32_bpb_t *bpb = (fat32_bpb_t *)sector;
    if (bpb->bytes_per_sector != ATA_SECTOR_SIZE)
        return 0;
    if (bpb->sectors_per_cluster == 0 || bpb->fats == 0)
        return 0;
    if (bpb->fat_size_16 != 0 || bpb->fat_size_32 == 0)
        return 0;
    if (!mem_eq(bpb->fs_type, "FAT32   ", 8))
        return 0;

    disk_index = dev;
    partition_lba = lba;
    sectors_per_cluster = bpb->sectors_per_cluster;
    sectors_per_fat = bpb->fat_size_32;
    root_cluster = bpb->root_cluster;
    fat_lba = partition_lba + bpb->reserved_sectors;
    data_lba = fat_lba + (uint32_t)bpb->fats * sectors_per_fat;
    mounted = 1;

    const ata_device_t *ata = ata_get_device(dev);
    str_copy(mount_name, ata ? ata->model : "ATA disk", sizeof(mount_name));
    return 1;
}

int fat32_init(void)
{
    uint8_t sector[ATA_SECTOR_SIZE];
    mounted = 0;

    for (int dev = 0; dev < ata_device_count(); ++dev) {
        if (!ata_read_sectors(dev, 0, 1, sector))
            continue;

        if (sector[510] == 0x55 && sector[511] == 0xAA) {
            for (int p = 0; p < 4; ++p) {
                uint8_t *entry = &sector[446 + p * 16];
                uint8_t type = entry[4];
                uint32_t start = rd32(&entry[8]);

                if ((type == 0x0B || type == 0x0C) && start != 0) {
                    if (mount_at(dev, start))
                        return 1;
                }
            }
        }

        if (mount_at(dev, 0))
            return 1;
    }

    return 0;
}

int fat32_is_mounted(void)
{
    return mounted;
}

const char *fat32_mount_name(void)
{
    return mounted ? mount_name : "none";
}

int fat32_list_root(fat32_dir_entry_t *entries, int max_entries)
{
    if (!mounted || !entries || max_entries <= 0)
        return -1;

    uint8_t sector[ATA_SECTOR_SIZE];
    uint32_t cluster = root_cluster;
    int count = 0;

    while (!is_eoc(cluster)) {
        for (uint8_t s = 0; s < sectors_per_cluster; ++s) {
            if (!read_sector(cluster_to_lba(cluster) + s, sector))
                return -1;

            fat32_raw_entry_t *raw = (fat32_raw_entry_t *)sector;
            for (int i = 0; i < 16; ++i) {
                if ((uint8_t)raw[i].name[0] == 0x00)
                    return count;
                if (!usable_entry(&raw[i]))
                    continue;
                if (count >= max_entries)
                    return count;

                format_name(&raw[i], entries[count].name);
                entries[count].attr = raw[i].attr;
                entries[count].first_cluster =
                    ((uint32_t)raw[i].first_cluster_high << 16) |
                    raw[i].first_cluster_low;
                entries[count].size = raw[i].file_size;
                ++count;
            }
        }

        if (!read_fat_entry(cluster, &cluster))
            return -1;
    }

    return count;
}

int fat32_read_file(const char *name, uint8_t *buffer, uint32_t buffer_size, uint32_t *bytes_read)
{
    fat32_dir_entry_t entries[64];
    int count = fat32_list_root(entries, 64);
    uint32_t written = 0;

    if (bytes_read)
        *bytes_read = 0;
    if (count < 0 || !name || !buffer)
        return 0;

    for (int i = 0; i < count; ++i) {
        if (!name_matches(name, entries[i].name))
            continue;
        if (entries[i].attr & 0x10)
            return 0;

        uint8_t sector[ATA_SECTOR_SIZE];
        uint32_t cluster = entries[i].first_cluster;
        uint32_t remaining = entries[i].size;

        while (remaining > 0 && !is_eoc(cluster)) {
            for (uint8_t s = 0; s < sectors_per_cluster && remaining > 0; ++s) {
                if (!read_sector(cluster_to_lba(cluster) + s, sector))
                    return 0;

                uint32_t to_copy = remaining < ATA_SECTOR_SIZE ? remaining : ATA_SECTOR_SIZE;
                if (written + to_copy > buffer_size)
                    to_copy = buffer_size - written;

                for (uint32_t j = 0; j < to_copy; ++j)
                    buffer[written + j] = sector[j];

                written += to_copy;
                remaining -= remaining < ATA_SECTOR_SIZE ? remaining : ATA_SECTOR_SIZE;

                if (written == buffer_size && remaining > 0) {
                    if (bytes_read)
                        *bytes_read = written;
                    return 1;
                }
            }

            if (!read_fat_entry(cluster, &cluster))
                return 0;
        }

        if (bytes_read)
            *bytes_read = written;
        return 1;
    }

    return 0;
}

int fat32_create_file(const char *name)
{
    if (!mounted || !name)
        return 0;

    uint8_t sector[ATA_SECTOR_SIZE];
    char target[11];
    make_83_name(name, target);

    uint32_t cluster = root_cluster;
    uint32_t free_lba = 0;
    int free_index = -1;
    int end_reached = 0;

    while (!end_reached && !is_eoc(cluster)) {
        for (uint8_t s = 0; s < sectors_per_cluster; ++s) {
            uint32_t lba = cluster_to_lba(cluster) + s;
            if (!read_sector(lba, sector))
                return 0;

            fat32_raw_entry_t *raw = (fat32_raw_entry_t *)sector;
            for (int i = 0; i < 16; ++i) {
                uint8_t first = (uint8_t)raw[i].name[0];

                if (first == 0x00) {
                    if (free_index < 0) {
                        free_lba = lba;
                        free_index = i;
                    }
                    end_reached = 1;
                    break;
                }

                if (usable_entry(&raw[i]) && mem_eq(raw[i].name, target, 11))
                    return 0; /* file already exists */

                if (first == 0xE5 && free_index < 0) {
                    free_lba = lba;
                    free_index = i;
                }
            }

            if (end_reached)
                break;
        }

        if (!end_reached && !read_fat_entry(cluster, &cluster))
            return 0;
    }

    if (free_index < 0)
        return 0; /* directory full */

    if (!read_sector(free_lba, sector))
        return 0;

    fat32_raw_entry_t *raw = (fat32_raw_entry_t *)sector;
    for (int i = 0; i < 11; ++i)
        raw[free_index].name[i] = target[i];
    raw[free_index].attr = 0x20;
    raw[free_index].nt_reserved = 0;
    raw[free_index].create_time_tenth = 0;
    raw[free_index].create_time = 0;
    raw[free_index].create_date = 0;
    raw[free_index].access_date = 0;
    raw[free_index].first_cluster_high = 0;
    raw[free_index].write_time = 0;
    raw[free_index].write_date = 0;
    raw[free_index].first_cluster_low = 0;
    raw[free_index].file_size = 0;

    return write_sector(free_lba, sector);
}

int fat32_delete_file(const char *name)
{
    if (!mounted || !name)
        return 0;

    uint8_t sector[ATA_SECTOR_SIZE];
    char target[11];
    make_83_name(name, target);

    uint32_t cluster = root_cluster;
    uint32_t entry_lba = 0;
    int entry_index = -1;
    fat32_raw_entry_t found;

    while (!is_eoc(cluster)) {
        for (uint8_t s = 0; s < sectors_per_cluster; ++s) {
            uint32_t lba = cluster_to_lba(cluster) + s;
            if (!read_sector(lba, sector))
                return 0;

            fat32_raw_entry_t *raw = (fat32_raw_entry_t *)sector;
            for (int i = 0; i < 16; ++i) {
                if ((uint8_t)raw[i].name[0] == 0x00)
                    return 0;
                if (!usable_entry(&raw[i]))
                    continue;
                if (mem_eq(raw[i].name, target, 11)) {
                    entry_lba = lba;
                    entry_index = i;
                    found = raw[i];
                    goto found_entry;
                }
            }
        }

        if (!read_fat_entry(cluster, &cluster))
            return 0;
    }

    return 0;

found_entry:
    if (found.attr & 0x10)
        return 0; /* refuse to delete directories */

    {
        uint32_t chain_cluster =
            ((uint32_t)found.first_cluster_high << 16) | found.first_cluster_low;

        while (chain_cluster != 0 && !is_eoc(chain_cluster)) {
            uint32_t next;
            if (!read_fat_entry(chain_cluster, &next))
                return 0;
            if (!write_fat_entry(chain_cluster, 0))
                return 0;
            chain_cluster = next;
        }
    }

    if (!read_sector(entry_lba, sector))
        return 0;

    {
        fat32_raw_entry_t *raw = (fat32_raw_entry_t *)sector;
        raw[entry_index].name[0] = (char)0xE5;
        return write_sector(entry_lba, sector);
    }
}

int fat32_write_existing_file(const char *name, const uint8_t *buffer, uint32_t size)
{
    if (!mounted || !name || !buffer)
        return 0;

    uint8_t sector[ATA_SECTOR_SIZE];
    char target[11];
    make_83_name(name, target);

    uint32_t cluster = root_cluster;
    uint32_t entry_lba = 0;
    int entry_index = -1;
    fat32_raw_entry_t found;

    while (!is_eoc(cluster)) {
        for (uint8_t s = 0; s < sectors_per_cluster; ++s) {
            uint32_t lba = cluster_to_lba(cluster) + s;
            if (!read_sector(lba, sector))
                return 0;

            fat32_raw_entry_t *raw = (fat32_raw_entry_t *)sector;
            for (int i = 0; i < 16; ++i) {
                if ((uint8_t)raw[i].name[0] == 0x00)
                    return 0;
                if (!usable_entry(&raw[i]))
                    continue;
                if (mem_eq(raw[i].name, target, 11)) {
                    entry_lba = lba;
                    entry_index = i;
                    found = raw[i];
                    goto found_entry;
                }
            }
        }

        if (!read_fat_entry(cluster, &cluster))
            return 0;
    }

    return 0;

found_entry:
    if (found.attr & 0x10)
        return 0;

    uint32_t first_cluster =
        ((uint32_t)found.first_cluster_high << 16) | found.first_cluster_low;
    uint32_t chain_clusters = 0;
    cluster = first_cluster;

    while (!is_eoc(cluster)) {
        ++chain_clusters;
        if (!read_fat_entry(cluster, &cluster))
            return 0;
    }

    if (size > chain_clusters * sectors_per_cluster * ATA_SECTOR_SIZE)
        return 0;

    uint8_t out[ATA_SECTOR_SIZE];
    uint32_t written = 0;
    cluster = first_cluster;

    while (!is_eoc(cluster)) {
        for (uint8_t s = 0; s < sectors_per_cluster; ++s) {
            for (uint32_t i = 0; i < ATA_SECTOR_SIZE; ++i)
                out[i] = 0;

            uint32_t remaining = size > written ? size - written : 0;
            uint32_t to_copy = remaining < ATA_SECTOR_SIZE ? remaining : ATA_SECTOR_SIZE;
            for (uint32_t i = 0; i < to_copy; ++i)
                out[i] = buffer[written + i];

            if (!write_sector(cluster_to_lba(cluster) + s, out))
                return 0;
            written += to_copy;
        }

        if (!read_fat_entry(cluster, &cluster))
            return 0;
    }

    if (!read_sector(entry_lba, sector))
        return 0;

    fat32_raw_entry_t *raw = (fat32_raw_entry_t *)sector;
    raw[entry_index].file_size = size;

    return write_sector(entry_lba, sector);
}