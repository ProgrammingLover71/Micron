#include "command.h"
#include "../vga/vga.h"
#include "../utils.h"
#include "../shell/shell_str.h"
#include "../fs/ata.h"
#include "../fs/fat32.h"
#include "edit/edit.h"


static int cmd_help(int argc, char **argv);
static int cmd_clear(int argc, char **argv);
static int cmd_reboot(int argc, char **argv);
static int cmd_shutdown(int argc, char **argv);
static int cmd_echo(int argc, char **argv);
static int cmd_uver(int argc, char **argv);
static int cmd_disks(int argc, char **argv);
static int cmd_ls(int argc, char **argv);
static int cmd_rm(int argc, char **argv);
static int cmd_touch(int argc, char **argv);
static int cmd_cat(int argc, char **argv);
static int cmd_edit(int argc, char **argv);

// --- Command Registry --- //

static const rt_command_t commands[] = {
    {"help",     "Show this message",          cmd_help},
    {"clear",    "Clear the screen",           cmd_clear},
    {"reboot",   "Reboot the system",          cmd_reboot},
    {"echo",     "Display a line of text",     cmd_echo},
    {"shutdown", "Shutdown the system",        cmd_shutdown},
    {"uver",     "Show version info",          cmd_uver},
    {"disks",    "List ATA disks",             cmd_disks},
    {"ls",       "List root directory",        cmd_ls},
    {"rm",       "Delete a text file",         cmd_rm},
    {"touch",    "Create a text file",         cmd_touch},
    {"cat",      "Print a text file",          cmd_cat},
    {"edit",     "Edit a text file",           cmd_edit},
};


const rt_command_t *rt_get_commands(void)
{
    return commands;
}


int rt_get_command_count(void)
{
    return (int)(sizeof(commands) / sizeof(commands[0]));
}


// --- Commands --- //


static int cmd_help(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    vga_puts("Commands:\n");
    for (int i = 0; i < rt_get_command_count(); ++i) {
        const rt_command_t *cmd = &commands[i];
        vga_puts("  ");
        vga_puts(cmd->name);
        vga_puts(" - ");
        vga_puts(cmd->help);
        vga_puts("\n");
    }
    return 0;
}

static int cmd_clear(int argc, char **argv)
{
    (void)argv;
    if (argc > 1) return 1;
    
    vga_clear();
    return 0;
}


static int cmd_reboot(int argc, char **argv)
{
    (void)argv;
    if (argc > 1) return 1;

    vga_puts("Rebooting...\n");

    uint8_t temp;
    do {
        __asm__ volatile ("inb %1, %0"
            : "=a"(temp)
            : "Nd"(0x64));
    } while (temp & 0x02);

    outb(0x64, 0xFE);

    for (;;)
        __asm__ volatile ("hlt");

    return 0;
}


static void bios_poweroff(void)
{
    __asm__ volatile (
        "pushl %%ebx\n\t"
        "movl $0x5307, %%eax\n\t"
        "movl $0x0001, %%ebx\n\t"
        "movl $0x0003, %%ecx\n\t"
        "int $0x15\n\t"
        "popl %%ebx\n\t"
        :
        :
        : "eax", "ebx", "ecx", "edx", "memory");
}


static int cmd_shutdown(int argc, char **argv)
{
    (void)argv;
    if (argc > 1) return 1;

    vga_puts("Shutting down...\n");
    bios_poweroff();
    vga_puts("Shutdown request failed; halting.\n");

    for (;;)
        __asm__ volatile ("hlt");

    return 0;
}


static int cmd_echo(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i) {
        if (i > 1)
            vga_putc(' ');
        vga_puts(argv[i]);
    }
    vga_puts("\n");
    return 0;
}


static int cmd_uver(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    vga_puts("\n   Micron OS v1.0.b01\n");
    vga_puts("   Micron Shell v1.0\n");
    vga_puts("   (c) 2026 Beef Steak\n\n");
    return 0;
}

static void uint_to_str(uint32_t value, char *buf, int buf_size)
{
    char tmp[16];
    int len = 0;

    if (!buf || buf_size <= 1)
        return;

    do {
        tmp[len++] = (char)('0' + (value % 10));
        value /= 10;
    } while (value > 0 && len < (int)sizeof(tmp));

    if (len >= buf_size)
        len = buf_size - 1;

    for (int i = 0; i < len; ++i)
        buf[i] = tmp[len - 1 - i];
    buf[len] = '\0';
}

static int cmd_disks(int argc, char **argv)
{
    (void)argv;
    if (argc > 1) return 1;

    char num[16];
    int count = ata_device_count();
    vga_puts("ATA disks:\n");
    for (int i = 0; i < count; ++i) {
        const ata_device_t *dev = ata_get_device(i);
        vga_puts("  ata");
        uint_to_str((uint32_t)i, num, sizeof(num));
        vga_puts(num);
        vga_puts(": ");
        vga_puts(dev->model[0] ? dev->model : "unknown");
        vga_puts(" (");
        uint_to_str(dev->sectors / 2048, num, sizeof(num));
        vga_puts(num);
        vga_puts(" MiB)\n");
    }
    if (count == 0)
        vga_puts("  none\n");

    vga_puts("FAT32: ");
    vga_puts(fat32_is_mounted() ? fat32_mount_name() : "not mounted");
    vga_puts("\n");
    return 0;
}

static int cmd_ls(int argc, char **argv)
{
    (void)argv;
    if (argc > 1) return 1;
    if (!fat32_is_mounted()) {
        vga_puts("No FAT32 filesystem mounted.\n");
        return 1;
    }

    fat32_dir_entry_t entries[64];
    int count = fat32_list_root(entries, 64);
    if (count < 0) {
        vga_puts("Could not read FAT32 directory.\n");
        return 1;
    }

    char num[16];
    for (int i = 0; i < count; ++i) {
        vga_puts("  ");
        vga_puts(entries[i].name);
        if (entries[i].attr & 0x10) {
            vga_puts(" <DIR>");
        } else {
            vga_puts(" ");
            uint_to_str(entries[i].size, num, sizeof(num));
            vga_puts(num);
            vga_puts(" bytes");
        }
        vga_puts("\n");
    }
    if (count == 0)
        vga_puts("  empty\n");
    return 0;
}

static int cmd_touch(int argc, char **argv)
{
    if (argc != 2) {
        vga_puts("Usage: touch FILE.TXT\n");
        return 1;
    }
    if (!fat32_is_mounted()) {
        vga_puts("No FAT32 filesystem mounted.\n");
        return 1;
    }

    if (!fat32_create_file(argv[1])) {
        vga_puts("Could not create file.\n");
        return 1;
    }

    vga_puts("File created successfully: ");
    vga_puts(argv[1]);
    vga_puts("\n");
    return 0;
}

static int cmd_rm(int argc, char **argv)
{
    if (argc != 2) {
        vga_puts("Usage: rm FILE.TXT\n");
        return 1;
    }
    if (!fat32_is_mounted()) {
        vga_puts("No FAT32 filesystem mounted.\n");
        return 1;
    }

    if (!fat32_delete_file(argv[1])) {
        vga_puts("Could not delete file.\n");
        return 1;
    }

    vga_puts("File deleted successfully: ");
    vga_puts(argv[1]);
    vga_puts("\n");
    return 0;
}

static int cmd_cat(int argc, char **argv)
{
    if (argc != 2) return 1;
    if (!fat32_is_mounted()) {
        vga_puts("No FAT32 filesystem mounted.\n");
        return 1;
    }

    static uint8_t buffer[4096];
    uint32_t bytes = 0;
    if (!fat32_read_file(argv[1], buffer, sizeof(buffer), &bytes)) {
        vga_puts("Could not read file.\n");
        return 1;
    }

    for (uint32_t i = 0; i < bytes; ++i)
        vga_putc((char)buffer[i]);

    if (bytes == sizeof(buffer))
        vga_puts("\n[truncated]\n");
    else
        vga_puts("\n");
    return 0;
}

static int cmd_edit(int argc, char **argv)
{
    if (argc != 2) {
        vga_puts("Usage: edit FILE.TXT\n");
        return 1;
    }

    return edit_run(argv[1]);
}
