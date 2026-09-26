; This chapter's real #GP (General Protection Fault, vector 13) stub.
; Structurally identical to 027_isr14.asm's own #PF stub, for the same
; reason: #GP is one of the exceptions that pushes a real 32-bit error
; code (OSDev Wiki, "Exceptions", vector 13's Error Code column reads
; "Yes"), sitting on the stack just above the eight registers PUSHA
; saves, which this stub reads, passes to C, and discards with its own
; `add esp, 4` before IRET -- IRET has no idea a fourth value is
; sitting underneath the three (or, for a ring 3 -> 0 -> 3 round trip
; like this chapter's own CLI demo, five) values it actually pops.
BITS 32

section .text
extern isr13_handler
global isr13
isr13:
    pusha                    ; save eax, ecx, edx, ebx, esp, ebp, esi, edi
    mov eax, [esp+32]        ; the CPU's error code sits right above the
                              ; eight 4-byte registers pusha just pushed
    push eax                 ; pass it as isr13_handler's one cdecl argument
    call isr13_handler
    add esp, 4                ; cdecl: caller cleans up its own pushed argument
    popa                      ; restore every register pusha saved
    add esp, 4                ; discard the CPU's own error code -- IRET
                              ; does not expect it and does not pop it
    iret                      ; this fault happened while ring-3 code was
                              ; running (027_kmain.c's own CLI demo is the
                              ; only thing in this chapter that can raise
                              ; it), so this IRET is itself a real
                              ; privilege-changing one -- but isr13_handler
                              ; never returns (see its own comments), so
                              ; this line is unreachable in this chapter's
                              ; own real run
