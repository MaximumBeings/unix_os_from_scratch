; Chapter 2: the Multiboot2 header GRUB scans for, plus the tiny assembly
; entry point every C function in this book ultimately runs under. GRUB
; hands control to _start in 32-bit protected mode with no stack set up
; and no guarantee the .bss segment has been zeroed by anything but us --
; so this file's only two jobs are: point esp at a real stack, then call
; into C.
BITS 32

section .multiboot_header
align 8
header_start:
    dd 0xe85250d6                ; magic number (multiboot2)
    dd 0                         ; architecture 0 (protected mode i386)
    dd header_end - header_start ; header length
    dd 0x100000000 - (0xe85250d6 + 0 + (header_end - header_start)) ; checksum
    ; end tag
    dw 0
    dw 0
    dd 8
header_end:

section .text
extern kmain
global _start
_start:
    mov esp, stack_top
    call kmain
    cli
.hang:
    hlt
    jmp .hang

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:
