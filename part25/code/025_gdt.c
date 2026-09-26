/* Chapter 5 gave this kernel three descriptors: null, kernel code,
 * kernel data -- a flat memory model where every earlier chapter's
 * code, no matter which ring it thought it might someday run at, has
 * always actually run at ring 0. This chapter adds the three real
 * descriptors that make ring 3 possible at all: a ring-3 code
 * segment, a ring-3 data segment (OSDev Wiki, "Getting to Ring 3":
 * "add 2 new segments to your GDT... with a base of 0, a limit of
 * 0xFFFFFFFF, and a privilege level of 3"), and one real Task State
 * Segment descriptor -- without which, per the same page, "it is
 * impossible to return to ring 0 for system calls, faults, or even
 * IRQs." */

#include <stdint.h>

#include "025_gdt.h"
#include "025_tss.h"

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

/* 0: null, 1: kernel code, 2: kernel data (all unchanged since
 * Chapter 5), 3: user code, 4: user data, 5: this chapter's TSS. */
#define GDT_ENTRY_COUNT 6
static struct gdt_entry gdt_entries[GDT_ENTRY_COUNT];
static struct gdt_ptr   gdt_pointer;

extern void gdt_flush(uint32_t gdt_ptr_addr);

static void gdt_set_entry(int index, uint32_t base, uint32_t limit,
                           uint8_t access, uint8_t flags) {
    gdt_entries[index].base_low    = (uint16_t) (base & 0xFFFF);
    gdt_entries[index].base_middle = (uint8_t)  ((base >> 16) & 0xFF);
    gdt_entries[index].base_high   = (uint8_t)  ((base >> 24) & 0xFF);

    gdt_entries[index].limit_low   = (uint16_t) (limit & 0xFFFF);
    gdt_entries[index].granularity = (uint8_t) (((limit >> 16) & 0x0F) | (flags & 0xF0));

    gdt_entries[index].access = access;
}

void gdt_init(void) {
    /* Entry 0: the mandatory null descriptor, unchanged since Ch5. */
    gdt_set_entry(0, 0, 0, 0, 0);

    /* Entry 1: kernel code, ring 0, flat 4 GiB, unchanged since Ch5.
     * Access byte 0x9A: P=1, DPL=00, S=1, E=1, DC=0, RW=1, A=0. */
    gdt_set_entry(1, 0, 0xFFFFF, 0x9A, 0xC0);

    /* Entry 2: kernel data, ring 0, flat 4 GiB, unchanged since Ch5.
     * Access byte 0x92: same as entry 1 but E=0 (data, not code). */
    gdt_set_entry(2, 0, 0xFFFFF, 0x92, 0xC0);

    /* Entry 3: user code, ring 3, flat 4 GiB -- identical to entry 1
     * except DPL (bits 6-5): 11 instead of 00. P=1,DPL=11,S=1,E=1,
     * DC=0,RW=1,A=0 packed high-to-low: 1 11 1 1 0 1 0 = 0xFA. */
    gdt_set_entry(3, 0, 0xFFFFF, 0xFA, 0xC0);

    /* Entry 4: user data, ring 3, flat 4 GiB -- identical to entry 2
     * except DPL=11: 1 11 1 0 0 1 0 = 0xF2. */
    gdt_set_entry(4, 0, 0xFFFFF, 0xF2, 0xC0);

    /* Entry 5: this chapter's one TSS. Access byte 0x89 -- cited
     * directly (OSDev Wiki, "Task State Segment"): "0x89
     * (Present|Executable|Accessed) as access byte and 0x40
     * (Size-bit) as flags." tss_init() must already have run before
     * this point, so tss_get_address()/tss_get_size() describe a
     * real, already-zeroed structure, not garbage memory. */
    tss_init();
    gdt_set_entry(5, (uint32_t) (uintptr_t) tss_get_address(),
                  tss_get_size() - 1, 0x89, 0x40);

    gdt_pointer.limit = (uint16_t) (sizeof(gdt_entries) - 1);
    gdt_pointer.base  = (uint32_t) &gdt_entries;

    gdt_flush((uint32_t) &gdt_pointer);

    /* LTR is only valid once the GDT it reads from is the one
     * actually loaded -- gdt_flush() above (LGDT, then reloading
     * every segment register) has to happen first. GDT index 5,
     * 5 * 8 = 0x28. */
    tss_load(0x28);
}
