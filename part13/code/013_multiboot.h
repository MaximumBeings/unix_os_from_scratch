#ifndef UNIX_OS_013_MULTIBOOT_H
#define UNIX_OS_013_MULTIBOOT_H

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
 * mboot_info_addr (the physical address this book's own 013_boot.asm
 * handed straight into kmain from EBX) looking for the one tag this
 * chapter actually needs -- type 6, the memory map -- and returns a
 * pointer to it, or 0 if this boot information structure genuinely has
 * none. */
const struct multiboot_tag_mmap *multiboot_find_mmap(uint32_t mboot_info_addr);

/* Prints every real entry the found memory map tag contains -- base
 * address, length, and type -- exactly as GRUB/the firmware reported
 * them, unfiltered. */
void multiboot_print_mmap(const struct multiboot_tag_mmap *mmap);

#endif
