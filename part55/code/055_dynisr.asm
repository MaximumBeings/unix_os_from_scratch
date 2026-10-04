; This chapter's own new, minimal software-interrupt stub, used only
; to PROVE idt_install_gate()'s own real dynamism against a vector
; nothing else in this book has ever used -- 0x90, chosen only because
; it falls safely outside every gate this book's own idt_init() or any
; driver installs. Structurally identical to 055_isr0.asm's own real
; stub (not a real CPU exception, so no error code, no privilege
; change -- this book only ever executes `int 0x90` from ring 0).
BITS 32

section .text
extern dynisr_demo_handler
global dynisr_demo
dynisr_demo:
    pusha
    call dynisr_demo_handler
    popa
    iret
