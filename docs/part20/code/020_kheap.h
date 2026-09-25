#ifndef UNIX_OS_020_KHEAP_H
#define UNIX_OS_020_KHEAP_H

#include <stdint.h>

/* Sets up this kernel's first dynamic memory allocator: maps an
 * initial run of virtual pages at KHEAP_START (via this chapter's own
 * paging_map_page(), backed by frames from Chapter 7's
 * pmm_alloc_frame()) and installs one large free block covering all
 * of it. */
void kheap_init(void);

/* Returns a pointer to at least `size` usable bytes, or 0 if the heap
 * could not be grown far enough to satisfy the request. */
void *kmalloc(uint32_t size);

/* Returns a block previously handed out by kmalloc() to the free
 * list, coalescing it with any free neighbor blocks. */
void kfree(void *ptr);

/* Prints every block currently on the heap's list -- address, size,
 * and free/used state -- in address order. Exists purely for this
 * chapter's own real, live verification: splitting, coalescing, and
 * growth are all things this function lets a real run show happening,
 * rather than merely claiming to. */
void kheap_dump(void);

#endif
