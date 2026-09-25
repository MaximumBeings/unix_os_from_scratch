; Chapter 26: the real machine code the CPU jumps to on IRQ11 -- this
; kernel's first-ever handler for a slave-PIC line (8-15), but
; structurally identical to Chapter 5/6's own irq1/irq0 stubs, and for
; the same reasons: the CPU is the "caller" here, with no cooperation
; from whatever code it interrupted, so every register is saved and
; restored by hand, and IRET (not a plain RET) is what correctly
; unwinds the frame the CPU itself pushed. Hardware IRQs push no error
; code, exactly like IRQ0/IRQ1, so this stub needs no extra stack
; cleanup either -- and nothing about being a slave-PIC line changes
; any of that at the CPU's own level; 029_pic.c's own pic_send_eoi()
; is what actually has to know the difference, not this stub.
BITS 32

section .text
extern irq11_handler
global irq11
irq11:
    pusha
    call irq11_handler
    popa
    iret
