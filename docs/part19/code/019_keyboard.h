#ifndef UNIX_OS_019_KEYBOARD_H
#define UNIX_OS_019_KEYBOARD_H

/* Unmasks IRQ1 on the PIC (019_pic.c) so real keyboard interrupts can
 * start arriving. Call only after pic_remap() and pic_disable_all()
 * have already run, and before this kernel enables interrupts globally
 * with STI. */
void keyboard_init(void);

/* The real C handler 019_irq1.asm's stub calls on every IRQ1. Declared
 * here for the same reason Chapter 4's isr_handlers.h declares
 * isr0_handler: nothing in C calls it directly, but every module in
 * this book still defines against its own header. */
void irq1_handler(void);

#endif
