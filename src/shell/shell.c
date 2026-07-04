#include "shell.h"
#include "shell_str.h"
#include "../rt/command.h"

static char *shell_prompt = " >> ";

static int parse_command(const char *cmd, int *argc, char **argv)
{
    static char buffer[128];
    static char *args[16];
    int count = 0;

    if (!cmd || !argc || !argv)
        return 0;

    for (int i = 0; i < (int)sizeof(buffer); ++i)
        buffer[i] = 0;

    for (int i = 0; cmd[i] != '\0' && i < (int)sizeof(buffer) - 1; ++i)
        buffer[i] = cmd[i];

    char *p = buffer;
    while (*p == ' ')
        p++;

    if (*p == '\0') {
        *argc = 0;
        return 1;
    }

    args[count++] = p;
    for (char *cur = p; *cur != '\0'; ++cur) {
        if (*cur == ' ') {
            *cur = '\0';
            char *next = cur + 1;
            while (*next == ' ')
                next++;
            if (*next != '\0' && count < 16)
                args[count++] = next;
        }
    }

    *argc = count;
    for (int i = 0; i < count; ++i)
        argv[i] = args[i];
    argv[count] = 0;
    return 1;
}

static int handle_command(const char *cmd)
{
    if (cmd[0] == '\0')
        return 0;

    int argc = 0;
    char *argv[16] = {0};
    if (!parse_command(cmd, &argc, argv))
        return 0;

    const rt_command_t *commands = rt_get_commands();
    int count = rt_get_command_count();

    for (int i = 0; i < count; ++i) {
        if (str_eq(commands[i].name, argv[0])) {
            int code = commands[i].handler(argc, argv);
            return code;
        }
    }

    vga_puts("Unknown command '");
    vga_puts(argv[0]);
    vga_puts("'. Check if it is spelled correctly.\n");
    return -1;
}

void shell_run()
{
    // Display welcome message
    vga_puts("Micron Shell v1.0 (Micron OS v1.01)\n");
    vga_puts("Type 'help' for a list of commands.\n");

    char line[128];

    for (;;) 
    {
        int len = 0;
        vga_puts(shell_prompt);

        while (1) 
        {
            char c = keyboard_getchar();

            if (c == '\n' || c == '\r') 
            {
                vga_puts("\n");
                line[len] = '\0';
                break;
            }

            if (c == '\b') 
            {
                if (len > 0) 
                {
                    len--;
                    vga_putc('\b');
                }
                continue;
            }

            if (len < (int)sizeof(line) - 1) 
            {
                line[len++] = c;
                vga_putc(c);
            }
        }

        int code = handle_command(line);
        if (code != 0) 
        {
            vga_puts("Error - '");
            vga_puts(line);
            vga_puts("' exited with code ");

            char code_str[12];
            int_to_str(code, code_str, sizeof(code_str));
            vga_puts(code_str);

            vga_puts("\n");
        }
    }
}