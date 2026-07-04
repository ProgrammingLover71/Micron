#include "mem.h"
#include "../vga/vga.h"
#include "../shell/shell.h"
#include "../fs/ata.h"
#include "../fs/fat32.h"

void kernel_main(void)
{
    vga_init();
    vga_puts("[ Kernel ]: VGA initialized.\n");
    
    keyboard_init();
    vga_puts("[ Kernel ]: Keyboard initialized.\n");

    ata_init();
    vga_puts("[ Kernel ]: ATA initialized.\n");
    if (fat32_init())
        vga_puts("[ Kernel ]: FAT32 mounted.\n");
    else
        vga_puts("[ Kernel ]: No FAT32 volume found.\n");
    vga_puts("\n");
    
    // Run the shell
    shell_run();

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
