/* Chapter 3: this book's own Global Descriptor Table (GDT). GRUB already
 * left the CPU in 32-bit protected mode with *some* valid GDT loaded --
 * that is one of the real machine-state guarantees the Multiboot2 spec
 * makes, and it is why every chapter so far has been able to run at all.
 * But that GDT belongs to GRUB, not to this kernel: its exact contents
 * are not part of any spec this book can rely on, and nothing stops a
 * later chapter (or GRUB itself, on a different build) from shaping it
 * differently. This chapter installs a GDT this kernel owns and fully
 * understands, with exactly three descriptors: the mandatory null
 * descriptor, one flat 4 GiB kernel code segment, and one flat 4 GiB
 * kernel data segment -- a "flat memory model," where segmentation is
 * present (x86 protected mode cannot fully turn it off) but does no real
 * work, because every segment covers the entire address space. */

#include <stdint.h>

#include "003_gdt.h"

/* One 8-byte GDT entry, packed field-by-field exactly as the hardware
 * reads it (OSDev Wiki, "GDT Tutorial", encodeGdtEntry): limit bits
 * 0-15, base bits 0-23 split across three bytes, one access byte, one
 * byte whose upper nibble is flags (G/DB/L/AVL) and whose lower nibble
 * is limit bits 16-19, then base bits 24-31. */
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

/* The structure LGDT actually loads: a 16-bit limit (the GDT's size in
 * bytes, minus one) followed by a 32-bit linear base address -- 48 bits
 * total (OSDev Wiki, "GDT Tutorial": "gdtr DW 0 ; For limit storage /
 * DD 0 ; For base storage"). */
struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

#define GDT_ENTRY_COUNT 3
static struct gdt_entry gdt_entries[GDT_ENTRY_COUNT];
static struct gdt_ptr   gdt_pointer;

/* Implemented in 003_gdt_flush.asm: loads gdt_pointer with LGDT, then
 * reloads every segment register -- CS included, via a far jump, since
 * "changing the CS register requires code resembling a jump or call to
 * elsewhere, as this is the only way its value is meant to be changed"
 * (OSDev Wiki, "GDT Tutorial"). */
extern void gdt_flush(uint32_t gdt_ptr_addr);

static void gdt_set_entry(int index, uint32_t base, uint32_t limit,
                           uint8_t access, uint8_t flags) {
    gdt_entries[index].base_low    = (uint16_t) (base & 0xFFFF);
    gdt_entries[index].base_middle = (uint8_t)  ((base >> 16) & 0xFF);
    gdt_entries[index].base_high   = (uint8_t)  ((base >> 24) & 0xFF);

    gdt_entries[index].limit_low   = (uint16_t) (limit & 0xFFFF);
    /* Upper nibble = flags (G, DB, L, AVL); lower nibble = limit bits
     * 16-19, per the byte layout cited above. */
    gdt_entries[index].granularity = (uint8_t) (((limit >> 16) & 0x0F) | (flags & 0xF0));

    gdt_entries[index].access = access;
}

void gdt_init(void) {
    /* Entry 0: the mandatory null descriptor. The CPU requires selector
     * 0 to be invalid, so segment registers can be zeroed to mean "no
     * segment" -- this entry's contents are never read as a real
     * segment. */
    gdt_set_entry(0, 0, 0, 0, 0);

    /* Entry 1: kernel code, ring 0, flat 4 GiB. The access byte's bits,
     * cited from the OSDev Wiki's own access-byte table ("Global
     * Descriptor Table"):
     *   P  (bit 7) = 1  -- "Must be set (1) for any valid segment."
     *   DPL(6-5)   = 00 -- "0 = highest privilege (kernel)"
     *   S  (bit 4) = 1  -- "it defines a code or data segment"
     *   E  (bit 3) = 1  -- "it defines a code segment which can be
     *                       executed from"
     *   DC (bit 2) = 0  -- non-conforming: only ring 0 may execute it
     *   RW (bit 1) = 1  -- "read access is allowed" (code needs this to
     *                       let the CPU fetch literals stored in .text)
     *   A  (bit 0) = 0  -- accessed bit, cleared until the CPU sets it
     * packed high-to-low: 1 0 0 1 1 0 1 0 = 0x9A. */
    gdt_set_entry(1, 0, 0xFFFFF, 0x9A, 0xC0);

    /* Entry 2: kernel data, ring 0, flat 4 GiB. Same P/DPL/S bits as
     * entry 1, but E (bit 3) = 0 -- "it defines a data segment" -- and
     * bit 1 becomes the writable bit (data needs write access for a
     * stack and globals to work at all): 1 0 0 1 0 0 1 0 = 0x92. */
    gdt_set_entry(2, 0, 0xFFFFF, 0x92, 0xC0);

    /* Flags nibble (upper nibble of the granularity byte), cited from
     * the same access/flags reference:
     *   G  = 1 -- "the Limit is in 4 KiB blocks" (0xFFFFF limit units
     *              of 4 KiB covers the full 4 GiB address space)
     *   DB = 1 -- "it defines a 32-bit protected mode segment"
     *   L  = 0 -- not a 64-bit long-mode code segment
     *   AVL= 0 -- unused by this kernel
     * packed as the upper nibble: 1100 = 0xC, shifted up 4 bits -> 0xC0,
     * matching the 0xC0 passed to gdt_set_entry above. */

    gdt_pointer.limit = (uint16_t) (sizeof(gdt_entries) - 1);
    gdt_pointer.base  = (uint32_t) &gdt_entries;

    gdt_flush((uint32_t) &gdt_pointer);
}
