#ifndef UNIX_OS_012_SERIAL_H
#define UNIX_OS_012_SERIAL_H

/* This book's first shared low-level header: Chapters 1 and 2 each kept
 * their own private copy of this exact code, because nothing yet needed
 * it from more than one file at once. This chapter's own kprintf does --
 * it has to reach the COM1 UART from 012_printf.c without duplicating
 * this code a third time -- so the promised refactor happens now. */
void serial_init(void);
void serial_putc(char c);
void serial_puts(const char *s);

#endif
