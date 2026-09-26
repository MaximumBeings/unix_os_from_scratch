#ifndef UNIX_OS_005_IDT_H
#define UNIX_OS_005_IDT_H

/* Installs this book's IDT: Chapter 4's own #DE handler at vector 0,
 * unchanged, plus this chapter's new real gate at vector 0x21 -- IRQ1,
 * the keyboard, after 005_pic.c has remapped the PIC so that vector
 * number is not still colliding with a CPU exception. */
void idt_init(void);

#endif
