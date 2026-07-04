# Editor

Run the editor from the shell:

```text
edit README.TXT
```

Normal mode:

- `h`, `j`, `k`, `l` or arrow keys move the cursor.
- `i` enters insert mode at the cursor.
- `a` enters insert mode after the cursor.
- `o` opens a new line below and enters insert mode.
- `x` deletes the character under the cursor.
- `dd` deletes the current line.
- `:` enters command mode.

Insert mode:

- Type text normally.
- `Enter` splits the line.
- `Backspace` deletes backward.
- `Esc` returns to normal mode.

Command mode:

- `:w` writes the file.
- `:q` quits if there are no unsaved changes.
- `:q!` quits and discards changes.
- `:wq` writes and quits.

Saving currently updates an existing FAT32 root-directory file in place. The new file contents must fit in the file's already allocated cluster chain; creating or expanding files needs FAT allocation support.
