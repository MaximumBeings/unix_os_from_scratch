#ifndef UNIX_OS_004_IDT_H
#define UNIX_OS_004_IDT_H

/* Installs this book's own Interrupt Descriptor Table and wires up a
 * real handler for CPU exception vector 0, #DE (Divide Error). See
 * 004_idt.c for why an IDT is needed at all, and 004_isr0.asm for why
 * the handler cannot be written as an ordinary C function. */
void idt_init(void);

#endif
