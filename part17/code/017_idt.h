#ifndef UNIX_OS_017_IDT_H
#define UNIX_OS_017_IDT_H

/* Installs this book's IDT: every gate through vector 0x21 unchanged
 * since Chapters 4-10, plus this chapter's two new real gates --
 * vector 13 (#GP, the General Protection Fault this chapter's own
 * ring-3 CLI demo deliberately triggers) and vector 0x80 (this
 * chapter's own syscall entry point, the first gate in this book with
 * DPL=3 instead of DPL=0, since ring-3 code has to be allowed to
 * execute `int 0x80` itself). */
void idt_init(void);

#endif
