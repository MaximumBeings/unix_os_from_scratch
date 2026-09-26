#ifndef UNIX_OS_029_ELF_H
#define UNIX_OS_029_ELF_H

#include <stdint.h>

/* This chapter's own real 32-bit ELF header, quoted field-for-field
 * from the OSDev Wiki ("ELF": the ELF Header table). Every field this
 * book actually reads is kept; e_shoff/e_shentsize/e_shnum/e_shstrndx
 * (the section header table) are kept too, purely for layout fidelity
 * with the real spec -- this loader never touches sections at all, the
 * same "segments are what get loaded, sections are for linkers and
 * debuggers" distinction the cited page itself draws. */
struct elf32_header {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed));

/* This chapter's own real 32-bit ELF Program Header, quoted the same
 * way (OSDev Wiki, "ELF": the Program Header table). One of these
 * exists per real loadable-or-otherwise segment; this loader only
 * ever acts on the ones whose p_type is PT_LOAD. */
struct elf32_phdr {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} __attribute__((packed));

/* "1 = load - clear p_memsz bytes at p_vaddr to 0, then copy p_filesz
 * bytes from p_offset to p_vaddr" (OSDev Wiki, "ELF": Segment Types) --
 * the one p_type value this chapter's loader ever acts on. */
#define PT_LOAD 1u

/* p_flags bit meanings, same citation: "Permissions (1=executable,
 * 2=writable, 4=readable)". This loader only ever consults PF_W, to
 * decide whether a segment's own mapped pages get PAGE_RW -- every
 * page this loader ever maps is PAGE_USER regardless of p_flags,
 * since every segment here belongs to ring-3 code by construction. */
#define PF_W 0x2u

/* e_machine's own real value for this book's own architecture (OSDev
 * Wiki, "ELF": the e_machine value table, x86 row -- "0x03"). */
#define ELF_MACHINE_386 0x03u

/* e_type's own real value for an ordinary executable (OSDev Wiki,
 * "ELF": the e_type field, "2 = executable"). */
#define ELF_TYPE_EXEC 2u

/* Reads and maps a real ELF executable's every PT_LOAD segment into
 * `dir_phys` -- a page directory built by 029_paging.c's own
 * paging_new_address_space(), not yet the active one -- straight out
 * of the raw module bytes at physical range [module_start, module_end)
 * (029_multiboot.h's own multiboot_tag_module, GRUB's own real load).
 * Validates the ELF magic bytes, class, type, and machine before
 * trusting anything else in the header; refuses (returns 0) rather
 * than mapping a byte of anything that fails any of those checks, or
 * any segment whose own page count exceeds ELF_MAX_SEGMENT_PAGES.
 * On success, writes the file's own real entry point (e_entry) to
 * `out_entry` and returns 1. */
#define ELF_MAX_SEGMENT_PAGES 8u

int elf_load(uint32_t dir_phys, uint32_t module_start, uint32_t module_end, uint32_t *out_entry);

#endif
