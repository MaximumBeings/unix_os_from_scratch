#ifndef UNIX_OS_012_ISR_HANDLERS_H
#define UNIX_OS_012_ISR_HANDLERS_H

#include <stdint.h>

/* The real C handler 012_isr0.asm's stub calls on a #DE fault. Nothing
 * in C calls it directly -- only the assembly stub does -- so this
 * declaration exists purely so 012_isr_handlers.c has a header to
 * define against, the same one-module-one-header shape every other
 * file in this book already follows. */
void isr0_handler(void);

/* The real C handler 012_isr14.asm's stub calls on a #PF fault, with
 * the CPU's own real error code passed as its one argument -- the
 * stub itself cannot decode the error code or read CR2, since neither
 * is something a plain assembly stub has any business interpreting. */
void isr14_handler(uint32_t error_code);

#endif
