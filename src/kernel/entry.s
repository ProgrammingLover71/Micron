; kernel/entry.s - 32-bit kernel entry point, loaded at 0x1000 by boot.s
[bits 32]

global _start
extern kernel_main

section .text.start

_start:
    ; Write X to the top-left to check if we are in protected mode
    mov byte [0xB8000], 'X'
    mov byte [0xB8001], 0x0F
    
    mov esp, stack_top   ; set up kernel's own stack
    call kernel_main
    cli
.hang:
    hlt
    jmp .hang

section .bss
align 16
stack_bottom:
    resb 16384            ; 16 KiB stack
stack_top: