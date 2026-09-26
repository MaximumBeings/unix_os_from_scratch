#ifndef UNIX_OS_033_PRINTF_H
#define UNIX_OS_033_PRINTF_H

/* A small, real printf-family function this kernel owns outright -- no
 * libc, no hosted <stdio.h>, just this book's own formatting logic on
 * top of the serial and VGA drivers already built. Supports %d, %u, %x,
 * %c, %s, %%, and (new this chapter) %llx for a real 64-bit value in
 * hexadecimal -- grown only because this chapter's own Multiboot2 memory
 * map entries are genuinely 64-bit (`addr`/`len`), and truncating them
 * through a 32-bit %x would silently discard real address bits on any
 * machine with more than 4 GiB of memory below a given region. Nothing
 * else -- no width/precision/padding specifiers -- grown further only
 * when a later chapter genuinely needs more. */
void kprintf(const char *fmt, ...);

#endif
