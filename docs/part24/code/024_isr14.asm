; Chapter 10: the real machine code the CPU jumps to on a #PF (Page
; Fault). Unlike #DE at vector 0 (009_isr0.asm), the CPU pushes a real
; 32-bit error code for this exception before jumping here (OSDev
; Wiki, "Exceptions": vector 14's Error Code column reads "Yes") --
; so this stub cannot simply pusha/popa/iret the way isr0's did. The
; error code sits on the stack just above the eight registers pusha
; pushes, and this stub reads it there, passes it as a real cdecl
; argument to isr14_handler, and -- critically -- discards it with its
; own `add esp, 4` before IRET, since IRET itself only ever pops
; EIP/CS/EFLAGS and has no idea a fourth value is sitting underneath
; them.
BITS 32

section .text
extern isr14_handler
global isr14
isr14:
    pusha                    ; save eax, ecx, edx, ebx, esp, ebp, esi, edi
    mov eax, [esp+32]        ; the CPU's error code sits right above the
                              ; eight 4-byte registers pusha just pushed
    push eax                 ; pass it as isr14_handler's one cdecl argument
    call isr14_handler
    add esp, 4                ; cdecl: caller cleans up its own pushed argument
    popa                      ; restore every register pusha saved
    add esp, 4                ; discard the CPU's own error code -- IRET
                              ; does not expect it and does not pop it
    iret                      ; pops EIP, CS, and EFLAGS, exactly what
                              ; remains on the stack now
