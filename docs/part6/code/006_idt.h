#ifndef UNIX_OS_006_IDT_H
#define UNIX_OS_006_IDT_H

/* Installs this book's IDT: Chapter 4's own #DE handler at vector 0 and
 * Chapter 5's own IRQ1 (keyboard) gate at vector 0x21, both unchanged,
 * plus this chapter's new real gate at vector 0x20 -- IRQ0, the PIT
 * timer, after 006_pic.c has remapped the PIC so that vector number is
 * not still colliding with a CPU exception. */
void idt_init(void);

#endif
