; Chapter 7: the same Multiboot2 header and stack-setup shape every chapter
; since Chapter 1 has used, plus exactly one new real job -- this book's
; kmain has never before needed anything the CPU itself did not already
; hand it, but a physical memory map is not something a kernel can invent;
; it only exists because GRUB already built it and left two real,
; specified values sitting in EAX and EBX the instant it jumped here.
; This chapter's own code is the first to read either register.
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

    ; "EAX must contain the magic value 0x36d76289 ... EBX must contain
    ; the 32-bit physical address of the Multiboot2 information
    ; structure" (GNU Multiboot2 Specification, "Machine state":
    ; https://www.gnu.org/software/grub/manual/multiboot2/multiboot.html).
    ; Both registers are only valid right here, before anything else runs
    ; and potentially clobbers them -- so this stub's only new job this
    ; chapter is handing them into C immediately, as real arguments, cdecl
    ; style: the rightmost parameter is pushed first, so kmain(magic,
    ; mboot_info_addr) needs ebx pushed before eax.
    push ebx
    push eax
    call kmain

    cli
.hang:
    hlt
    jmp .hang

; Chapter 38's own real, booted finding: kmain() itself has grown, over 38
; chapters' worth of never-refactored inline demo code, into one single
; giant function whose own -O0 stack frame -- sized to fit every local
; variable textually declared anywhere in its body, all at once, the
; standard non-optimizing codegen this book has used since Chapter 1 --
; already consumes most of a 16 KiB stack before a single nested call
; runs. Chapter 35's own investing_demo() (FIX + HMAC + AES-128-CBC,
; several call levels deep) then pushes the real stack pointer PAST
; stack_bottom, into this exact page's own .data section, real memory
; this chapter is the first to place anything else at, so it is also
; the first to ever notice: every earlier chapter's own boot silently
; overwrote the very first few bytes of what was, until now, always the
; start of an unused .bss. 64 KiB, found by real measurement (this
; chapter's own booted instrumentation printed real esp register values,
; see the chapter's own write-up), leaves comfortable real headroom.
section .bss
align 16
stack_bottom:
    resb 65536
stack_top:
