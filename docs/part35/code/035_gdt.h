#ifndef UNIX_OS_035_GDT_H
#define UNIX_OS_035_GDT_H

/* Installs this book's own GDT: Chapter 5's null/kernel-code/
 * kernel-data descriptors, unchanged, plus three new ones this
 * chapter needs to run anything at ring 3 at all -- a ring-3 code
 * segment, a ring-3 data segment, and this chapter's one real Task
 * State Segment descriptor. Also initializes and loads that TSS
 * (035_tss.c) -- the GDT and the TSS are installed together here
 * because the TSS descriptor's own address has to already be a real,
 * fixed struct before this function builds the GDT entry that points
 * at it, and LTR (035_tss.c's tss_load()) is only valid once THIS
 * exact GDT is the one actually loaded. */
void gdt_init(void);

#endif
