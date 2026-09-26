/* Chapter 7: the first code in this book to read anything GRUB left
 * behind other than the two machine-state registers Chapter 1 already
 * used to get here. The Multiboot2 information structure is real data,
 * built by the real bootloader, sitting in real physical memory --
 * this file's only job is walking it correctly, using nothing but the
 * two numbers the spec itself guarantees: total_size and each tag's own
 * type/size. */

#include <stdint.h>

#include "018_multiboot.h"
#include "018_printf.h"

const struct multiboot_tag_mmap *multiboot_find_mmap(uint32_t mboot_info_addr) {
    /* "The fixed part [...] consists of two fields: total_size [...]
     * contains the total size of boot information [...] and reserved,
     * which is always set to zero" (GNU Multiboot2 Specification, "Boot
     * information format"). Only total_size matters here; reserved is
     * read past, never used, exactly as the spec requires. */
    const uint32_t *header = (const uint32_t *) mboot_info_addr;
    uint32_t total_size = header[0];

    const uint8_t *tag_ptr = (const uint8_t *) (mboot_info_addr + 8);
    const uint8_t *end = (const uint8_t *) (mboot_info_addr + total_size);

    while (tag_ptr < end) {
        const uint32_t *tag_header = (const uint32_t *) tag_ptr;
        uint32_t type = tag_header[0];
        uint32_t size = tag_header[1];

        if (type == 0) {
            /* "Tags are terminated by a tag of type 0 and size 8" -- the
             * real end of this structure, not a bug if reached. */
            break;
        }
        if (type == 6) {
            return (const struct multiboot_tag_mmap *) tag_ptr;
        }

        /* "Tags follow one another padded when necessary in order for
         * each tag to start at 8-bytes aligned address" -- round this
         * tag's own real size up to the next multiple of 8 before
         * advancing, rather than assuming every tag is already aligned. */
        uint32_t advance = (size + 7u) & ~7u;
        tag_ptr += advance;
    }

    return 0;
}

const struct multiboot_tag_module *multiboot_find_module(uint32_t mboot_info_addr) {
    /* Structurally identical to multiboot_find_mmap() above -- the
     * exact same fixed-header-then-tag-stream walk, just watching for
     * type 3 (module) instead of type 6 (memory map). Duplicated
     * rather than factored into one shared "find tag by type" helper:
     * this book has kept every tag-finder this simple and specific
     * since Chapter 7, on the theory that a five-line loop is cheaper
     * to read twice than a generic one is to understand once. */
    const uint32_t *header = (const uint32_t *) mboot_info_addr;
    uint32_t total_size = header[0];

    const uint8_t *tag_ptr = (const uint8_t *) (mboot_info_addr + 8);
    const uint8_t *end = (const uint8_t *) (mboot_info_addr + total_size);

    while (tag_ptr < end) {
        const uint32_t *tag_header = (const uint32_t *) tag_ptr;
        uint32_t type = tag_header[0];
        uint32_t size = tag_header[1];

        if (type == 0) {
            break;
        }
        if (type == 3) {
            return (const struct multiboot_tag_module *) tag_ptr;
        }

        uint32_t advance = (size + 7u) & ~7u;
        tag_ptr += advance;
    }

    return 0;
}

void multiboot_print_mmap(const struct multiboot_tag_mmap *mmap) {
    /* entry_size is read from the tag itself, not assumed to be
     * sizeof(struct multiboot_mmap_entry) -- the spec allows a bootloader
     * to report a larger entry_size than this book's own struct, with
     * extra trailing fields this kernel does not know about, and reading
     * entries by stride rather than by sizeof() is what stays correct
     * either way. */
    uint32_t entry_count = (mmap->size - sizeof(struct multiboot_tag_mmap)) / mmap->entry_size;
    const uint8_t *entry_ptr = (const uint8_t *) mmap->entries;

    kprintf("Multiboot2 memory map: %u real entries, %u bytes each\n", entry_count, mmap->entry_size);

    for (uint32_t i = 0; i < entry_count; i++) {
        const struct multiboot_mmap_entry *entry = (const struct multiboot_mmap_entry *) entry_ptr;
        kprintf("  base 0x%llx  length 0x%llx  type %u%s\n",
                entry->addr, entry->len, entry->type,
                entry->type == MULTIBOOT_MEMORY_AVAILABLE ? " (available)" : "");
        entry_ptr += mmap->entry_size;
    }
}
