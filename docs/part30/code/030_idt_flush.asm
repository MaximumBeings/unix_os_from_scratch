; Chapter 5: loading the IDT is a single real instruction, unlike
; Chapter 3's GDT install -- LIDT takes effect immediately for the next
; interrupt or exception the CPU takes, with no segment registers to
; reload and no far jump required. CS is not involved in which IDT is
; loaded; it only matters later, at dispatch time, when the CPU checks
; whether the code that is about to run is allowed to take the
; interrupt at all.
BITS 32

section .text
global idt_flush
idt_flush:
    mov eax, [esp + 4]     ; cdecl: the one argument (an idt_ptr*) is on
                            ; the stack, 4 bytes above the return address
    lidt [eax]              ; load IDTR from the idt_ptr struct this
                            ; points at (2-byte limit, 4-byte base)
    ret
