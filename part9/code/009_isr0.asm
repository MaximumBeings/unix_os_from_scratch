; Chapter 5: the real machine code the CPU jumps to on a #DE (Divide
; Error) fault -- this cannot be an ordinary C function, for two real
; reasons. First, the CPU does not call it with the normal cdecl
; convention this book's C functions all assume (arguments on the
; stack, a plain `ret` to return); it pushes its own fixed set of
; values (EFLAGS, CS, EIP, and -- for some vectors, not this one --
; an error code) and expects the handler to leave via IRET, a real
; instruction with no C equivalent. Second, an ordinary C function is
; free to clobber any general-purpose register it wants, because the
; normal calling convention guarantees the caller already saved
; whatever it still needed -- but the "caller" here is the CPU itself,
; interrupting whatever this kernel was doing at an arbitrary
; instruction, so every register genuinely has to be saved and
; restored by hand.
BITS 32

section .text
extern isr0_handler
global isr0
isr0:
    pusha                   ; save eax, ecx, edx, ebx, esp, ebp, esi, edi
    call isr0_handler        ; ordinary cdecl call into real C
    popa                     ; restore every register pusha just saved
    iret                     ; #DE pushes no error code (OSDev Wiki,
                              ; "Interrupt Descriptor Table": vector 0
                              ; lists "No" under Error Code), so no
                              ; extra stack cleanup is needed before
                              ; this -- IRET pops EIP, CS, and EFLAGS
                              ; itself, exactly what the CPU pushed
