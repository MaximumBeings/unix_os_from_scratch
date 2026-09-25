; Chapter 6: the real machine code the CPU jumps to on IRQ0 -- structurally
; identical to Chapter 5's irq1 stub, and for the same reasons: the CPU
; is the "caller" here, with no cooperation from whatever code it
; interrupted, so every register is saved and restored by hand, and
; IRET (not a plain RET) is what correctly unwinds the frame the CPU
; itself pushed.
BITS 32

section .text
extern irq0_handler
global irq0
irq0:
    pusha
    call irq0_handler
    popa
    iret
