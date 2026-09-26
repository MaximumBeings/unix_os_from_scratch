#ifndef UNIX_OS_014_IDT_H
#define UNIX_OS_014_IDT_H

/* Installs this book's IDT: Chapter 4's own #DE handler at vector 0,
 * Chapter 5's own IRQ1 (keyboard) gate at vector 0x21, and Chapter
 * 6's own IRQ0 (PIT) gate at vector 0x20, all unchanged, plus this
 * chapter's new real gate at vector 14 -- #PF, the page fault
 * exception paging has been capable of raising since Chapter 8, but
 * that nothing installed a real handler for until now. */
void idt_init(void);

#endif
