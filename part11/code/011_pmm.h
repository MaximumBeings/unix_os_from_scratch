#ifndef UNIX_OS_011_PMM_H
#define UNIX_OS_011_PMM_H

#include <stdint.h>

#include "011_multiboot.h"

/* This chapter's own physical memory manager: a real bit-per-frame
 * bitmap, one bit for every 4 KiB frame in the range this kernel has
 * chosen to manage. Seeded from the real Multiboot2 memory map --
 * nothing in this bitmap's initial state is invented. */
void pmm_init(const struct multiboot_tag_mmap *mmap, uint32_t kernel_start, uint32_t kernel_end_addr);

/* Returns the physical address of one free 4 KiB frame, and marks it
 * used -- or 0 if this bitmap has no free frame left to give out. 0 is a
 * safe sentinel here, not an ambiguous one: frame 0 (physical address 0)
 * is always marked used, by construction, since pmm_init never frees any
 * frame below the 1 MiB mark. */
uint32_t pmm_alloc_frame(void);

/* Marks a previously allocated frame free again. */
void pmm_free_frame(uint32_t addr);

/* How many frames this bitmap currently has marked free -- used here
 * purely for this chapter's own verification output. */
uint32_t pmm_count_free_frames(void);

#endif
