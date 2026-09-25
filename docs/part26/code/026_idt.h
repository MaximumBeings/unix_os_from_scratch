#ifndef UNIX_OS_026_IDT_H
#define UNIX_OS_026_IDT_H

/* Installs this book's IDT: every gate through vector 0x80 unchanged
 * since Chapter 15, plus this chapter's own one new real gate --
 * vector 0x2B (IRQ11), this book's first gate for a slave-PIC line,
 * installed for this chapter's own new interrupt-driven RTL8139
 * driver (026_rtl8139.h/.c). */
void idt_init(void);

#endif
