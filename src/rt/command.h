#pragma once

typedef int (*rt_command_handler_t)(int argc, char **argv);

typedef struct {
    const char *name;
    const char *help;
    rt_command_handler_t handler;
} rt_command_t;

const rt_command_t *rt_get_commands(void);
int rt_get_command_count(void);
