# FAT32 Storage

Micron keeps its boot image and FAT32 storage separate:

- `build/micron.img` contains only the boot sector and kernel.
- `build/fat32.img` is a separate raw disk image with an MBR and one FAT32 LBA partition.

QEMU runs both images as independent IDE disks:

```powershell
make run
```

Inside Micron, use:

```text
disks
ls
cat README.TXT
```

For real hardware, write `build/micron.img` to the boot disk and `build/fat32.img` to a separate ATA/SATA disk, or create an equivalent FAT32 partition on a second disk. The current driver is ATA PIO, so SATA controllers need legacy IDE compatibility mode enabled in firmware. Native AHCI, NVMe, and USB mass storage need additional controller drivers.
