/* Chapter 7: this kernel's first allocator of any kind -- not bytes, but
 * whole 4 KiB physical frames, tracked one bit at a time. Every fact this
 * file starts from is real: which physical ranges are "available RAM"
 * comes from the real Multiboot2 memory map Chapter 7's own
 * 012_multiboot.c already parsed; which physical range this kernel's own
 * image occupies comes from 012_linker.ld's own kernel_end symbol, not a
 * guess. */

#include <stdint.h>

#include "012_pmm.h"

#define FRAME_SIZE 4096u

/* This chapter manages physical memory up through 64 MiB -- chosen to
 * match this chapter's own QEMU verification run (`-m 64`) exactly, so
 * every frame this allocator can ever hand out is a frame this book's
 * own evidence actually exercised. A real machine (or QEMU instance)
 * with more RAM than this would report memory-map entries this bitmap
 * cannot represent; pmm_init below caps at that boundary rather than
 * overrunning it -- a stated scope limit for this chapter, not a bug, in
 * the same spirit as Chapter 5's own scancode table covering only the
 * alphabet. */
#define MAX_MANAGED_MEMORY (64u * 1024u * 1024u)
#define MAX_FRAMES (MAX_MANAGED_MEMORY / FRAME_SIZE)

static uint8_t frame_bitmap[MAX_FRAMES / 8];

static inline void bitmap_mark_used(uint32_t frame) {
    frame_bitmap[frame / 8] |= (uint8_t) (1u << (frame % 8));
}

static inline void bitmap_mark_free(uint32_t frame) {
    frame_bitmap[frame / 8] &= (uint8_t) ~(1u << (frame % 8));
}

static inline int bitmap_is_used(uint32_t frame) {
    return frame_bitmap[frame / 8] & (uint8_t) (1u << (frame % 8));
}

void pmm_init(const struct multiboot_tag_mmap *mmap, uint32_t kernel_start, uint32_t kernel_end_addr) {
    /* Start pessimistic: every frame this bitmap can represent begins
     * marked used. Only a real "available" memory-map entry, below,
     * earns a frame its free bit back. */
    for (uint32_t i = 0; i < MAX_FRAMES / 8; i++) {
        frame_bitmap[i] = 0xFFu;
    }

    uint32_t entry_count = (mmap->size - sizeof(struct multiboot_tag_mmap)) / mmap->entry_size;
    const uint8_t *entry_ptr = (const uint8_t *) mmap->entries;

    for (uint32_t i = 0; i < entry_count; i++) {
        const struct multiboot_mmap_entry *entry = (const struct multiboot_mmap_entry *) entry_ptr;

        if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
            uint64_t region_start = entry->addr;
            uint64_t region_end = entry->addr + entry->len;

            /* Below 1 MiB is real-mode/BIOS-era territory (the EBDA, the
             * VGA framebuffer window this book's own Chapter 2 already
             * relies on being exactly where it is, and similar) -- this
             * chapter never starts managing it, even where the memory
             * map itself reports it as "available." */
            if (region_start < 0x100000u) {
                region_start = 0x100000u;
            }
            if (region_end > MAX_MANAGED_MEMORY) {
                region_end = MAX_MANAGED_MEMORY;
            }

            if (region_start < region_end) {
                uint32_t first_frame = (uint32_t) (region_start / FRAME_SIZE);
                uint32_t last_frame_exclusive = (uint32_t) (region_end / FRAME_SIZE);
                for (uint32_t f = first_frame; f < last_frame_exclusive; f++) {
                    bitmap_mark_free(f);
                }
            }
        }

        entry_ptr += mmap->entry_size;
    }

    /* Whatever the memory map said, this kernel's own running image --
     * every byte of code and data this CPU might read or write on its
     * very next instruction -- is never a frame this allocator hands out. */
    uint32_t kernel_start_frame = kernel_start / FRAME_SIZE;
    uint32_t kernel_end_frame_exclusive = (kernel_end_addr + FRAME_SIZE - 1u) / FRAME_SIZE;
    if (kernel_end_frame_exclusive > MAX_FRAMES) {
        kernel_end_frame_exclusive = MAX_FRAMES;
    }
    for (uint32_t f = kernel_start_frame; f < kernel_end_frame_exclusive; f++) {
        bitmap_mark_used(f);
    }
}

uint32_t pmm_alloc_frame(void) {
    /* A plain linear scan -- O(n) in the number of managed frames, and
     * deliberately not anything cleverer. At 16384 frames this is a few
     * thousand instructions at worst, and this book has no allocation
     * workload yet that would make that cost real; a later chapter can
     * replace this with a free-list without changing this function's own
     * contract at all. */
    for (uint32_t f = 0; f < MAX_FRAMES; f++) {
        if (!bitmap_is_used(f)) {
            bitmap_mark_used(f);
            return f * FRAME_SIZE;
        }
    }
    return 0;
}

void pmm_free_frame(uint32_t addr) {
    uint32_t f = addr / FRAME_SIZE;
    if (f < MAX_FRAMES) {
        bitmap_mark_free(f);
    }
}

uint32_t pmm_count_free_frames(void) {
    uint32_t count = 0;
    for (uint32_t f = 0; f < MAX_FRAMES; f++) {
        if (!bitmap_is_used(f)) {
            count++;
        }
    }
    return count;
}
