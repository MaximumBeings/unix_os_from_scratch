; Chapter 5: the real machine code the CPU jumps to on IRQ1 -- structurally
; identical to Chapter 4's isr0 stub, and for the same reasons: the CPU
; is the "caller" here, with no cooperation from whatever code it
; interrupted, so every register is saved and restored by hand, and
; IRET (not a plain RET) is what correctly unwinds the frame the CPU
; itself pushed. Hardware IRQs push no error code, exactly like #DE did
; in Chapter 4, so this stub needs no extra stack cleanup either.
BITS 32

section .text
extern irq1_handler
global irq1
irq1:
    pusha
    call irq1_handler
    popa
    iret
