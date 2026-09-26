#ifndef UNIX_OS_014_GDT_H
#define UNIX_OS_014_GDT_H

/* Installs this book's own Global Descriptor Table, replacing whatever
 * transient GDT GRUB left behind. See 014_gdt.c for why that matters. */
void gdt_init(void);

#endif
