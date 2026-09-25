#ifndef UNIX_OS_030_PAGING_H
#define UNIX_OS_030_PAGING_H

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

/* New this chapter: three real address-space primitives, needed the
 * moment more than one page directory exists at once.
 *
 * paging_new_address_space() allocates a fresh page directory and
 * copies every one of the kernel's own 1024 real page-directory
 * entries into it -- not just the 16 built by paging_init(), but
 * whatever the kernel's own directory holds at the exact moment this
 * is called, kheap growth and all. Copying the ENTRY (a physical page-
 * table address), rather than cloning the table it points at, means
 * the new address space shares the very same underlying page tables
 * for every kernel-space mapping: a byte written into the kheap
 * through the kernel's own directory is visible through the new one
 * too, automatically, with no re-copying ever needed for an EXISTING
 * table. Returns the new directory's own physical address.
 *
 * paging_map_page_in() is paging_map_page() generalized to operate on
 * any directory, not only the one currently loaded into CR3 -- needed
 * to build a brand-new process's own private mappings before that
 * process's directory is ever made active at all. paging_map_page()
 * itself is now defined in terms of this one, always passing the
 * kernel's own directory.
 *
 * paging_translate_in() walks a given directory read-only and returns
 * the real physical address `vaddr` resolves to inside it, or 0 if
 * unmapped -- this chapter's own way of PROVING, from ring 0, that
 * the identical virtual address means something different in two
 * different processes' own directories. */
uint32_t paging_new_address_space(void);
void paging_map_page_in(uint32_t dir_phys, uint32_t vaddr, uint32_t paddr, uint32_t flags);
uint32_t paging_translate_in(uint32_t dir_phys, uint32_t vaddr);

#endif
