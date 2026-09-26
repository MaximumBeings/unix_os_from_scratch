; This chapter's real syscall entry point: INT 0x80, this book's own
; chosen software-interrupt vector (OSDev Wiki, "System Calls": "The
; most common way to implement system calls is using a software
; interrupt"). Unlike every OTHER gate in this book, this one's own
; IDT descriptor has DPL=3 (027_idt.c), which is what lets ring-3 code
; execute `int 0x80` at all without an immediate #GP -- the DPL check
; the CPU makes for a software INT is the numeric opposite of every
; other access check in this book: it requires CPL <= the gate's own
; DPL, not the other way around.
;
; Structurally this stub is identical to 027_isr0.asm's -- INT 0x80 is
; not a real CPU exception at all (it is an arbitrary vector number
; this book chose), so, like #DE, it never pushes an error code. What
; IS different, invisibly, is that this exact INT instruction is
; itself a real privilege-level change (ring 3 -> ring 0): the CPU
; already switched ESP/SS to this TSS's own ESP0/SS0 (027_tss.c) and
; pushed the caller's OLD SS and ESP alongside EIP/CS/EFLAGS before
; this stub's very first instruction ever ran -- and the final IRET
; below pops all five values back off, symmetrically, returning
; straight to ring 3. Neither push nor pop needs special-case code
; here; the CPU handles the extra two values on its own, exactly the
; same way 027_isr13.asm's own IRET does for the CLI demo.
;
; EAX and EBX are this chapter's own real calling convention (OSDev
; Wiki, "System Calls" cites Linux's own real i386 convention: "The
; Linux kernel gets its arguments in eax, ebx, ecx, edx, esi, edi, and
; ebp in that order" -- this book only ever needs the first two).
; PUSHA saves copies of both onto the stack without disturbing the
; live registers, so they can simply be read back out and forwarded
; as isr128_handler's own two cdecl arguments.
BITS 32

section .text
extern isr128_handler
global isr128
isr128:
    pusha                     ; save eax, ecx, edx, ebx, esp, ebp, esi, edi
    push ebx                  ; 2nd real argument to isr128_handler
    push eax                  ; 1st real argument: the syscall number
    call isr128_handler
    add esp, 8                 ; cdecl: caller cleans up both pushed args
    popa                       ; restore every register pusha saved
    iret                       ; pops EIP, CS, EFLAGS, ESP, SS -- landing
                                ; straight back in ring 3, right after the
                                ; INT 0x80 that got here
