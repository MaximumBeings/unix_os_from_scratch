#ifndef UNIX_OS_024_MULTIBOOT_H
#define UNIX_OS_024_MULTIBOOT_H

#include <stdint.h>

/* "EAX must contain the magic value 0x36d76289" (GNU Multiboot2
 * Specification, "Machine state") -- kmain checks the real value GRUB
 * left in EAX against this before trusting anything else it was handed. */
#define MULTIBOOT2_BOOTLOADER_MAGIC 0x36d76289u

/* One real memory map entry, quoted field-for-field from the spec's own
 * example code (GNU Multiboot2 Specification, "Boot information
 * format"): struct multiboot_mmap_entry { multiboot_uint64_t addr;
 * multiboot_uint64_t len; multiboot_uint32_t type; multiboot_uint32_t
 * zero; }. This book renames nothing else, but spells out "zero" as
 * "reserved" in its own comment below since that is what every other
 * tag/entry "reserved" field in this spec is called elsewhere. */
struct multiboot_mmap_entry {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t reserved; /* the spec's own field is literally named "zero" */
} __attribute__((packed));

/* Type 1 is the only value this chapter's allocator ever treats as
 * usable -- "available RAM" per the cited spec's own type table. Every
 * other value (ACPI reclaimable, reserved, defective, and so on) is left
 * alone: a stated scope limit, not a gap this driver failed to notice. */
#define MULTIBOOT_MEMORY_AVAILABLE 1u

/* The memory map tag container itself, type 6, quoted the same way:
 * struct multiboot_tag_mmap { multiboot_uint32_t type; multiboot_uint32_t
 * size; multiboot_uint32_t entry_size; multiboot_uint32_t entry_version;
 * struct multiboot_mmap_entry entries[0]; }. */
struct multiboot_tag_mmap {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    struct multiboot_mmap_entry entries[];
} __attribute__((packed));

/* Walks the real boot information structure GRUB built at
 * mboot_info_addr (the physical address this book's own 024_boot.asm
 * handed straight into kmain from EBX) looking for the one tag this
 * chapter actually needs -- type 6, the memory map -- and returns a
 * pointer to it, or 0 if this boot information structure genuinely has
 * none. */
const struct multiboot_tag_mmap *multiboot_find_mmap(uint32_t mboot_info_addr);

/* Prints every real entry the found memory map tag contains -- base
 * address, length, and type -- exactly as GRUB/the firmware reported
 * them, unfiltered. */
void multiboot_print_mmap(const struct multiboot_tag_mmap *mmap);

/* New this chapter: one real Multiboot2 boot MODULE tag, quoted
 * field-for-field from the spec's own struct layout (GNU Multiboot2
 * Specification, section 3.6.6, "Modules"):
 *
 *     +-------------------+
 *     u32     | type = 3          |
 *     u32     | size              |
 *     u32     | mod_start         |
 *     u32     | mod_end           |
 *     u8[n]   | string            |
 *     +-------------------+
 *
 * "This tag indicates to the kernel what boot module was loaded along
 * with the kernel image, and where it can be found." "The 'mod_start'
 * and 'mod_end' contain the start and end physical addresses of the
 * boot module itself." Both are real physical addresses GRUB already
 * loaded the module's raw bytes at, before this kernel ever ran a
 * single instruction -- exactly the same "the bootloader already did
 * the real work" spirit Chapter 1 established for the kernel image
 * itself. `string` is this book's own new user program's real ELF
 * filename, passed through unread by this chapter (this book only
 * ever loads the one module it expects, by position, not by name). */
struct multiboot_tag_module {
    uint32_t type;
    uint32_t size;
    uint32_t mod_start;
    uint32_t mod_end;
    char string[];
} __attribute__((packed));

/* Walks the same boot information structure multiboot_find_mmap()
 * does, looking for the first tag of type 3 instead of type 6. "One
 * tag appears per module. This tag type may appear multiple times" --
 * this book only ever loads exactly one module, so the FIRST type-3
 * tag found is the only one this function ever needs to return.
 * Returns 0 if this boot information structure has no module tag at
 * all -- for instance, if 024_kmain.c's own real run were ever booted
 * from an ISO whose grub.cfg forgot its `module2` line. */
const struct multiboot_tag_module *multiboot_find_module(uint32_t mboot_info_addr);

#endif
