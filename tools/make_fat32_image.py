import os
import struct
import sys


SECTOR = 512
IMAGE_SECTORS = 32768
PARTITION_LBA = 2048
PARTITION_SECTORS = IMAGE_SECTORS - PARTITION_LBA
RESERVED = 32
FATS = 2
SECTORS_PER_CLUSTER = 1
FAT_SECTORS = 256
ROOT_CLUSTER = 2


def short_entry(name, ext, attr, cluster, size):
    raw_name = (name.upper().ljust(8)[:8] + ext.upper().ljust(3)[:3]).encode("ascii")
    return struct.pack(
        "<11sBBBHHHHHHHI",
        raw_name,
        attr,
        0,
        0,
        0,
        0,
        0,
        (cluster >> 16) & 0xFFFF,
        0,
        0,
        cluster & 0xFFFF,
        size,
    )


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "build/fat32.img"
    os.makedirs(os.path.dirname(out) or ".", exist_ok=True)

    image = bytearray(IMAGE_SECTORS * SECTOR)

    # MBR with one FAT32 LBA partition. This keeps the FAT disk separate from micron.img.
    image[446:462] = struct.pack(
        "<BBBBBBBBII",
        0x00,
        0x20,
        0x21,
        0x00,
        0x0C,
        0xFE,
        0xFF,
        0xFF,
        PARTITION_LBA,
        PARTITION_SECTORS,
    )
    image[510:512] = b"\x55\xAA"

    boot = PARTITION_LBA * SECTOR
    bpb = bytearray(SECTOR)
    bpb[0:3] = b"\xEB\x58\x90"
    bpb[3:11] = b"MICRON  "
    struct.pack_into("<H", bpb, 11, SECTOR)
    bpb[13] = SECTORS_PER_CLUSTER
    struct.pack_into("<H", bpb, 14, RESERVED)
    bpb[16] = FATS
    struct.pack_into("<H", bpb, 17, 0)
    struct.pack_into("<H", bpb, 19, 0)
    bpb[21] = 0xF8
    struct.pack_into("<H", bpb, 22, 0)
    struct.pack_into("<H", bpb, 24, 63)
    struct.pack_into("<H", bpb, 26, 255)
    struct.pack_into("<I", bpb, 28, PARTITION_LBA)
    struct.pack_into("<I", bpb, 32, PARTITION_SECTORS)
    struct.pack_into("<I", bpb, 36, FAT_SECTORS)
    struct.pack_into("<H", bpb, 40, 0)
    struct.pack_into("<H", bpb, 42, 0)
    struct.pack_into("<I", bpb, 44, ROOT_CLUSTER)
    struct.pack_into("<H", bpb, 48, 1)
    struct.pack_into("<H", bpb, 50, 6)
    bpb[64] = 0x80
    bpb[66] = 0x29
    struct.pack_into("<I", bpb, 67, 0x20260704)
    bpb[71:82] = b"MICRON FAT "
    bpb[82:90] = b"FAT32   "
    bpb[510:512] = b"\x55\xAA"
    image[boot:boot + SECTOR] = bpb

    fsinfo = bytearray(SECTOR)
    struct.pack_into("<I", fsinfo, 0, 0x41615252)
    struct.pack_into("<I", fsinfo, 484, 0x61417272)
    struct.pack_into("<I", fsinfo, 488, 0xFFFFFFFF)
    struct.pack_into("<I", fsinfo, 492, 4)
    fsinfo[510:512] = b"\x55\xAA"
    image[boot + SECTOR:boot + 2 * SECTOR] = fsinfo

    fat_start = boot + RESERVED * SECTOR

    fat = bytearray(FAT_SECTORS * SECTOR)
    entries = [0x0FFFFFF8, 0x0FFFFFFF, 0x0FFFFFFF, 0x0FFFFFFF]
    for i, value in enumerate(entries):
        struct.pack_into("<I", fat, i * 4, value)

    for f in range(FATS):
        start = fat_start + f * FAT_SECTORS * SECTOR
        image[start:start + FAT_SECTORS * SECTOR] = fat

    with open(out, "wb") as f:
        f.write(image)


if __name__ == "__main__":
    main()
