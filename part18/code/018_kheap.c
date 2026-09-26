#include <stdint.h>

#include "018_kheap.h"
#include "018_paging.h"
#include "018_pmm.h"
#include "018_printf.h"
#include "018_spinlock.h"

/* A real free-list heap allocator, in the same spirit the OSDev Wiki
 * describes: "put at the start of the freed zone a descriptor that
 * allows you to insert it in a list of free zones" -- next/prev
 * pointers and a size, kept sorted by address so neighbors can be
 * recognized and merged -- and "it's way easier to keep the size of
 * allocated objects in a header hidden from the requester, so that a
 * call to free doesn't require the object's size... kept just before
 * the block returned."
 * (OSDev Wiki, "Memory Allocation": https://wiki.osdev.org/Memory_Allocation)
 *
 * This chapter's own list stays sorted by construction: kheap_init()
 * starts with one block, kmalloc()'s splits only ever insert a new
 * block immediately after the one it came from, and kheap_expand()
 * only ever appends past the current tail -- so every insertion
 * already lands in address order, without a separate sort step.
 *
 * This chapter's own real, live verification (see the chapter text)
 * caught this exact code -- unchanged since Chapter 9 -- corrupting
 * this very free list for real, the moment two preemptible tasks
 * (Chapter 12) started calling kmalloc()/kfree() concurrently with no
 * synchronization at all. kheap_lock, below, is this chapter's fix:
 * every function that walks or mutates this free list now holds it
 * for the full duration of that walk or mutation. */
static spinlock_t kheap_lock;

#define KHEAP_START        0xD0000000u
#define KHEAP_INITIAL_PAGES 4u
#define KHEAP_MIN_SPLIT    16u
#define PAGE_SIZE_BYTES    4096u

typedef struct kheap_block {
    uint32_t size;   /* usable bytes in this block, not counting this header */
    uint32_t free;   /* 1 = free, 0 = handed out by kmalloc */
    struct kheap_block *next;
    struct kheap_block *prev;
} kheap_block_t;

static kheap_block_t *kheap_head = 0;
static kheap_block_t *kheap_tail = 0;
static uint32_t kheap_end = 0; /* first not-yet-mapped virtual address past the heap */

static uint32_t align_up8(uint32_t n) {
    return (n + 7u) & ~7u;
}

/* Maps `pages` fresh frames at the heap's current top, growing
 * kheap_end -- every one of these frames comes from Chapter 7's own
 * physical memory manager, and every mapping goes through this
 * chapter's own paging_map_page(), the same real functions the rest
 * of this kernel already depends on. */
static void kheap_map_pages(uint32_t pages) {
    for (uint32_t i = 0; i < pages; i++) {
        uint32_t frame = pmm_alloc_frame();
        paging_map_page(kheap_end, frame, PAGE_PRESENT | PAGE_RW);
        kheap_end += PAGE_SIZE_BYTES;
    }
}

void kheap_init(void) {
    spinlock_init(&kheap_lock);

    kheap_end = KHEAP_START;
    kheap_map_pages(KHEAP_INITIAL_PAGES);

    kheap_head = (kheap_block_t *) KHEAP_START;
    kheap_head->size = (KHEAP_INITIAL_PAGES * PAGE_SIZE_BYTES) - sizeof(kheap_block_t);
    kheap_head->free = 1;
    kheap_head->next = 0;
    kheap_head->prev = 0;
    kheap_tail = kheap_head;

    kprintf("kheap: initialized at 0x%x, %u bytes usable (%u pages mapped)\n",
            KHEAP_START, kheap_head->size, KHEAP_INITIAL_PAGES);
}

/* Grows the heap by enough whole pages to satisfy `min_size`, then
 * either extends the current tail block (if it is already free) or
 * appends a brand new free block covering the newly mapped pages --
 * the OSDev Wiki's own "if it's free" case for keeping the list
 * merged rather than fragmented, applied at grow time instead of only
 * at free time. */
static void kheap_expand(uint32_t min_size) {
    uint32_t needed = min_size + (uint32_t) sizeof(kheap_block_t);
    uint32_t pages = (needed + PAGE_SIZE_BYTES - 1u) / PAGE_SIZE_BYTES;
    if (pages == 0) {
        pages = 1;
    }

    uint32_t old_end = kheap_end;
    kheap_map_pages(pages);
    uint32_t grown_bytes = pages * PAGE_SIZE_BYTES;

    kprintf("kheap: growing by %u page(s) (%u bytes), old top 0x%x, new top 0x%x\n",
            pages, grown_bytes, old_end, kheap_end);

    if (kheap_tail != 0 && kheap_tail->free) {
        kheap_tail->size += grown_bytes;
        return;
    }

    kheap_block_t *new_block = (kheap_block_t *) old_end;
    new_block->size = grown_bytes - (uint32_t) sizeof(kheap_block_t);
    new_block->free = 1;
    new_block->next = 0;
    new_block->prev = kheap_tail;

    if (kheap_tail != 0) {
        kheap_tail->next = new_block;
    } else {
        kheap_head = new_block;
    }
    kheap_tail = new_block;
}

void *kmalloc(uint32_t size) {
    size = align_up8(size);

    /* Held for this whole call, split-and-return included: the exact
     * bug this chapter's own real run caught was another task's
     * kmalloc()/kfree() reading this same free list mid-split, while
     * some of this function's own writes to `b` and `new_block` had
     * already landed and others had not. Nothing below may run on any
     * other task's behalf until this one either returns a pointer or
     * gives up. */
    uint32_t saved_eflags = spinlock_acquire(&kheap_lock);

    for (int attempt = 0; attempt < 2; attempt++) {
        kheap_block_t *b = kheap_head;
        while (b != 0) {
            if (b->free && b->size >= size) {
                uint32_t leftover = b->size - size;
                if (leftover >= sizeof(kheap_block_t) + KHEAP_MIN_SPLIT) {
                    /* OSDev Wiki: "If the space found is significantly
                     * larger than the space needed... add another
                     * header right after the used space and update
                     * the pointers." (Writing a memory manager) */
                    kheap_block_t *new_block =
                        (kheap_block_t *) ((uint8_t *) (b + 1) + size);
                    new_block->size = leftover - (uint32_t) sizeof(kheap_block_t);
                    new_block->free = 1;
                    new_block->next = b->next;
                    new_block->prev = b;
                    if (b->next != 0) {
                        b->next->prev = new_block;
                    } else {
                        kheap_tail = new_block;
                    }
                    b->next = new_block;
                    b->size = size;
                }
                b->free = 0;
                spinlock_release(&kheap_lock, saved_eflags);
                return (void *) (b + 1);
            }
            b = b->next;
        }
        /* Nothing free was big enough -- grow the heap for real and
         * try exactly once more, rather than looping forever.
         * kheap_expand() runs with this same lock already held -- it
         * is only ever called from inside this critical section. */
        kheap_expand(size);
    }

    spinlock_release(&kheap_lock, saved_eflags);
    return 0;
}

void kfree(void *ptr) {
    if (ptr == 0) {
        return;
    }

    uint32_t saved_eflags = spinlock_acquire(&kheap_lock);

    kheap_block_t *b = ((kheap_block_t *) ptr) - 1;
    b->free = 1;

    /* OSDev Wiki: "if next pointer's header is free... set the
     * current block's next pointer to that used block, skipping over
     * the free blocks" -- merge forward first. (Writing a memory manager) */
    if (b->next != 0 && b->next->free) {
        kheap_block_t *n = b->next;
        b->size += (uint32_t) sizeof(kheap_block_t) + n->size;
        b->next = n->next;
        if (n->next != 0) {
            n->next->prev = b;
        } else {
            kheap_tail = b;
        }
    }

    /* Then merge backward into a free previous block, the same way. */
    if (b->prev != 0 && b->prev->free) {
        kheap_block_t *p = b->prev;
        p->size += (uint32_t) sizeof(kheap_block_t) + b->size;
        p->next = b->next;
        if (b->next != 0) {
            b->next->prev = p;
        } else {
            kheap_tail = p;
        }
    }

    spinlock_release(&kheap_lock, saved_eflags);
}

void kheap_dump(void) {
    /* Locked for the same reason kmalloc()/kfree() are: a torn
     * mid-write view of this list is exactly what produced this
     * chapter's own real corrupted dump (a block whose `next` pointed
     * back into itself, printed forever). This function only ever
     * reads the list, but reading it while another task is mid-write
     * is just as unsafe as two tasks writing it at once. */
    uint32_t saved_eflags = spinlock_acquire(&kheap_lock);

    kheap_block_t *b = kheap_head;
    uint32_t idx = 0;
    while (b != 0) {
        kprintf("  block %u: addr 0x%x size %u %s\n",
                idx, (uint32_t) (uintptr_t) (b + 1), b->size, b->free ? "FREE" : "USED");
        b = b->next;
        idx++;
    }

    spinlock_release(&kheap_lock, saved_eflags);
}
