#ifndef UNIX_OS_016_PAGING_H
#define UNIX_OS_016_PAGING_H

#include <stdint.h>

#define PAGE_PRESENT 0x1u
#define PAGE_RW      0x2u

/* New this chapter: bit 2 of a real x86 page-table (and page-
 * directory) entry -- "the U/S bit" -- 0 means "only CPL 0-2 (in
 * practice, this book's own CPL 0) may access this page," 1 means
 * "CPL 3 may access it too." Every page this kernel has ever mapped
 * before this chapter, including its own .text, was mapped without
 * this bit -- which was never a problem, because nothing before this
 * chapter ever ran at any CPL other than 0. */
#define PAGE_USER    0x4u

void paging_init(void);
void paging_map_page(uint32_t vaddr, uint32_t paddr, uint32_t flags);

#endif
