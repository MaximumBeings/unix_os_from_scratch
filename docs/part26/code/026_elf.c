#include <stdint.h>

#include "026_elf.h"
#include "026_paging.h"
#include "026_pmm.h"
#include "026_printf.h"

/* Physical addresses double as valid pointers everywhere this loader
 * looks -- both the raw module bytes (GRUB loaded them somewhere
 * inside the 0-64 MiB identity-mapped range this kernel has managed
 * since Chapter 7/8) and every frame pmm_alloc_frame() hands back --
 * the same invariant every other paging primitive in this book already
 * relies on. */
static inline uint8_t *phys_ptr8(uint32_t phys_addr) {
    return (uint8_t *) phys_addr;
}

static void zero_frame(uint32_t phys_addr) {
    uint8_t *p = phys_ptr8(phys_addr);
    for (uint32_t i = 0; i < 4096u; i++) {
        p[i] = 0;
    }
}

/* Maps and fills exactly one PT_LOAD segment: allocates however many
 * whole pages its own [p_vaddr, p_vaddr + p_memsz) range spans (a
 * segment's own p_vaddr is not guaranteed page-aligned by the ELF
 * format itself, only by this book's own user-program linker script --
 * this function does not assume that alignment, and computes real page
 * boundaries from the segment's own numbers either way), zeroes each
 * one BEFORE mapping it (so the tail of the last page, beyond
 * p_filesz, is real zero-filled BSS with no extra pass needed), copies
 * exactly p_filesz real bytes from the module's own file offset, and
 * maps every page into `dir_phys` with PAGE_USER always set and
 * PAGE_RW set only when the segment's own p_flags says PF_W (OSDev
 * Wiki, "ELF": "Permissions (1=executable, 2=writable, 4=readable)"). */
static int elf_load_segment(uint32_t dir_phys, uint32_t module_start,
                             const struct elf32_phdr *ph) {
    uint32_t page_start = ph->p_vaddr & ~0xFFFu;
    uint32_t span_end = ph->p_vaddr + ph->p_memsz;
    uint32_t page_end = (span_end + 0xFFFu) & ~0xFFFu;
    uint32_t page_count = (page_end - page_start) / 4096u;

    if (page_count == 0u || page_count > ELF_MAX_SEGMENT_PAGES) {
        kprintf("elf_load: segment at vaddr 0x%x spans %u page(s), outside 1..%u -- refusing\n",
                ph->p_vaddr, page_count, ELF_MAX_SEGMENT_PAGES);
        return 0;
    }

    uint32_t frames[ELF_MAX_SEGMENT_PAGES];
    for (uint32_t i = 0; i < page_count; i++) {
        frames[i] = pmm_alloc_frame();
        zero_frame(frames[i]);
    }

    /* "clear p_memsz bytes at p_vaddr to 0, then copy p_filesz bytes
     * from p_offset to p_vaddr" (OSDev Wiki, "ELF": Segment Types) --
     * the zeroing already happened above, one whole frame at a time;
     * this loop performs exactly the copy half, byte by byte, each
     * byte's own destination frame and in-page offset computed from
     * its real virtual address, never assuming p_filesz itself is a
     * multiple of the page size. */
    const uint8_t *src = phys_ptr8(module_start + ph->p_offset);
    for (uint32_t i = 0; i < ph->p_filesz; i++) {
        uint32_t vaddr = ph->p_vaddr + i;
        uint32_t page_index = (vaddr - page_start) / 4096u;
        uint32_t in_page_offset = vaddr & 0xFFFu;
        phys_ptr8(frames[page_index])[in_page_offset] = src[i];
    }

    uint32_t flags = PAGE_PRESENT | PAGE_USER;
    if (ph->p_flags & PF_W) {
        flags |= PAGE_RW;
    }
    for (uint32_t i = 0; i < page_count; i++) {
        paging_map_page_in(dir_phys, page_start + i * 4096u, frames[i], flags);
    }

    kprintf("elf_load: PT_LOAD vaddr=0x%x filesz=%u memsz=%u flags=%s -- %u page(s) mapped\n",
            ph->p_vaddr, ph->p_filesz, ph->p_memsz,
            (ph->p_flags & PF_W) ? "RW" : "R", page_count);

    return 1;
}

int elf_load(uint32_t dir_phys, uint32_t module_start, uint32_t module_end, uint32_t *out_entry) {
    if (module_end <= module_start || (module_end - module_start) < sizeof(struct elf32_header)) {
        kprintf("elf_load: module too small to hold an ELF header -- refusing\n");
        return 0;
    }

    const struct elf32_header *hdr = (const struct elf32_header *) phys_ptr8(module_start);

    /* "Magic number - 0x7F, then 'ELF' in ASCII" (OSDev Wiki, "ELF":
     * the ELF Header table, e_ident row) -- checked byte for byte,
     * never assumed from the file's own name or extension. */
    if (hdr->e_ident[0] != 0x7Fu || hdr->e_ident[1] != 'E' ||
        hdr->e_ident[2] != 'L' || hdr->e_ident[3] != 'F') {
        kprintf("elf_load: bad magic (0x%x 0x%x 0x%x 0x%x) -- not an ELF file, refusing\n",
                hdr->e_ident[0], hdr->e_ident[1], hdr->e_ident[2], hdr->e_ident[3]);
        return 0;
    }
    if (hdr->e_ident[4] != 1u) {
        kprintf("elf_load: e_ident[EI_CLASS]=%u, not 1 (32-bit) -- refusing\n", hdr->e_ident[4]);
        return 0;
    }
    if (hdr->e_type != ELF_TYPE_EXEC) {
        kprintf("elf_load: e_type=%u, not %u (ET_EXEC) -- refusing\n", hdr->e_type, ELF_TYPE_EXEC);
        return 0;
    }
    if (hdr->e_machine != ELF_MACHINE_386) {
        kprintf("elf_load: e_machine=0x%x, not 0x%x (x86) -- refusing\n",
                hdr->e_machine, ELF_MACHINE_386);
        return 0;
    }

    kprintf("elf_load: valid ELF32 executable, e_entry=0x%x, %u program header(s)\n",
            hdr->e_entry, hdr->e_phnum);

    uint32_t loaded_segments = 0;
    for (uint32_t i = 0; i < hdr->e_phnum; i++) {
        const uint8_t *ph_ptr = phys_ptr8(module_start + hdr->e_phoff) + (uint32_t) i * hdr->e_phentsize;
        const struct elf32_phdr *ph = (const struct elf32_phdr *) ph_ptr;

        if (ph->p_type != PT_LOAD) {
            continue;
        }
        if (!elf_load_segment(dir_phys, module_start, ph)) {
            return 0;
        }
        loaded_segments++;
    }

    if (loaded_segments == 0u) {
        kprintf("elf_load: no PT_LOAD segments found -- refusing\n");
        return 0;
    }

    *out_entry = hdr->e_entry;
    return 1;
}
