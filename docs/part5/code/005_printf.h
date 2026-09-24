#ifndef UNIX_OS_005_PRINTF_H
#define UNIX_OS_005_PRINTF_H

/* A small, real printf-family function this kernel owns outright -- no
 * libc, no hosted <stdio.h>, just this book's own formatting logic on
 * top of the serial and VGA drivers already built. Supports %d, %u, %x,
 * %c, %s, and a literal %%; nothing else (no width/precision/padding
 * specifiers) -- enough for this book's own debug output going forward,
 * grown further only when a later chapter genuinely needs more. */
void kprintf(const char *fmt, ...);

#endif
