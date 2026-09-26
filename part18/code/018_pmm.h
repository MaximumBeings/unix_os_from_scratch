#ifndef UNIX_OS_018_PMM_H
#define UNIX_OS_018_PMM_H

#include <stdint.h>

#include "018_multiboot.h"

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

/* New this chapter: marks every whole 4 KiB frame overlapping
 * [start, end) used, without ever allocating them through the ordinary
 * free-frame scan -- the one real gap this kernel's own physical memory
 * manager had left since Chapter 7. pmm_init() already reserves this
 * kernel's OWN image (kernel_start..kernel_end_addr), because that
 * range comes from this kernel's own linker script, known at compile
 * time. A real GRUB Multiboot2 MODULE (018_multiboot.h's own
 * multiboot_tag_module) is different: GRUB loads it at whatever real
 * physical address happens to be free at boot time, discovered only by
 * walking the boot information structure at RUN time -- pmm_init()
 * itself has no way to know that address in advance. Called with that
 * exact real [mod_start, mod_end) range, before this allocator ever
 * hands out a single frame, so nothing -- not paging_init()'s own
 * identity-map tables, not kheap_init(), not any task's own kernel
 * stack -- can ever be handed a frame this module's own raw bytes are
 * still sitting in, between the moment GRUB placed them there and the
 * moment 018_elf.c's own elf_load() finally reads them. */
void pmm_reserve_range(uint32_t start, uint32_t end);

/* How many frames this bitmap currently has marked free -- used here
 * purely for this chapter's own verification output. */
uint32_t pmm_count_free_frames(void);

#endif
