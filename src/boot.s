; boot.s - Minimal i386 MBR bootloader for Micron OS
[org 0x0600]
[bits 16]

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov si, 0x7C00
    mov di, 0x0600
    mov cx, 256
    cld
    rep movsw
    jmp 0x0000:relocated_start

relocated_start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0xF000       ; stay above the 0x1000 kernel load buffer
    mov [boot_drive], dl ; BIOS passes the boot disk in DL
    sti

    mov si, msg_kernel_load
    call print_string

    ; --- Load kernel from disk (LBA sectors right after boot sector) ---
    mov word [dap_offset], 0x1000
    mov word [dap_segment], 0x0000
    mov dword [dap_lba_low], 1
    mov dword [dap_lba_high], 0
    mov cx, num_kernel_sectors

load_kernel_sector:
    push cx
    mov ah, 0x42
    mov dl, [boot_drive]
    mov si, dap
    int 0x13
    pop cx
    jc disk_error

    add word [dap_offset], 512
    inc dword [dap_lba_low]
    loop load_kernel_sector

    mov si, msg_kernel_loaded
    call print_string

    mov si, msg_kernel_boot
    call print_string

    ; --- Enable A20 line (fast method) ---
    in al, 0x92
    or al, 2
    out 0x92, al

    cli
    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 1
    mov cr0, eax        ; enter protected mode


    jmp CODE_SEG:protected_mode_entry

disk_error:
    mov si, msg_disk_read_err
    call print_string
    mov si, msg_halt
    call print_string
    jmp $

print_string:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0E
    int 0x10
    jmp print_string
.done:
    ret

msg_kernel_load   db "[ Boot ] Loading kernel...", 13, 10, 0
msg_kernel_loaded db "[ Boot ] Kernel loaded.", 13, 10, 0
msg_kernel_boot   db "[ Boot ] Booting kernel...", 13, 10, 0

msg_disk_read_err db "[ Boot ] Failed to read kernel (disk read error).", 13, 10, 0
msg_halt          db 13, 10, "Aborting...", 13, 10, 0
boot_drive        db 0

dap:
    db 16
    db 0
dap_count:
    dw 1
dap_offset:
    dw 0
dap_segment:
    dw 0
dap_lba_low:
    dd 0
dap_lba_high:
    dd 0

; How many sectors to load for the kernel (after the boot sector)
; Keep this comfortably above the current kernel size. The boot image is padded
; to match, and the FAT32 disk is attached separately.
num_kernel_sectors equ 96

; --- GDT: flat memory model, code + data segments ---
gdt_start:
    dq 0x0                       ; null descriptor
gdt_code:
    dw 0xFFFF, 0x0000
    db 0x00, 10011010b, 11001111b, 0x00
gdt_data:
    dw 0xFFFF, 0x0000
    db 0x00, 10010010b, 11001111b, 0x00
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

[bits 32]
protected_mode_entry:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, 0x90000

    jmp 0x1000              ; jump into loaded kernel

; --- Boot sector padding + signature ---
times 510-($-$$) db 0
dw 0xAA55
