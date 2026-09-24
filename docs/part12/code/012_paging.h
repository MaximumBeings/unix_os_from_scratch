#ifndef UNIX_OS_012_PAGING_H
#define UNIX_OS_012_PAGING_H

#include <stdint.h>

#define PAGE_PRESENT 0x1u
#define PAGE_RW      0x2u

void paging_init(void);
void paging_map_page(uint32_t vaddr, uint32_t paddr, uint32_t flags);

#endif
