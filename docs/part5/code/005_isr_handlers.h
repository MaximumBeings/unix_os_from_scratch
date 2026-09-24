#ifndef UNIX_OS_005_ISR_HANDLERS_H
#define UNIX_OS_005_ISR_HANDLERS_H

/* The real C handler 005_isr0.asm's stub calls on a #DE fault. Nothing
 * in C calls it directly -- only the assembly stub does -- so this
 * declaration exists purely so 005_isr_handlers.c has a header to
 * define against, the same one-module-one-header shape every other
 * file in this book already follows. */
void isr0_handler(void);

#endif
