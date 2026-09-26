#include <stdint.h>

#include "020_paging.h"
#include "020_pmm.h"
#include "020_printf.h"

/* This chapter identity-maps the same 0-64 MiB range that chapter 7's
 * physical memory manager already manages (MAX_MANAGED_MEMORY there),
 * so every physical frame this kernel can ever hand out from
 * pmm_alloc_frame() already has a valid virtual address equal to its
 * own physical address. One page table covers 4 MiB (1024 entries *
 * 4 KiB), so 64 MiB needs exactly 16 page tables -- and 16 page
 * directory entries out of the 1024 this kernel's single page
 * directory has room for. */
#define IDENTITY_MAP_TABLES 16u
#define ENTRIES_PER_TABLE   1024u
#define PAGE_DIR_ENTRIES    1024u

/* The physical address of this kernel's one and only page directory,
 * set once by paging_init() and read by every later paging_map_page()
 * call. Storing only the physical address -- not a C pointer typed at
 * some virtual mapping -- is deliberate: this whole file only ever
 * touches memory that lives inside the identity-mapped 0-64 MiB
 * range, so a frame's physical address always doubles as a valid,
 * dereferenceable pointer, both before and after paging is switched
 * on. */
static uint32_t page_directory_phys = 0;

/* Physical addresses in this kernel are also valid pointers, but only
 * because everything this file touches lives inside the
 * identity-mapped range -- this helper exists to make that fact
 * explicit at every use, not to hide a general physical-to-virtual
 * translation this kernel does not have. */
static inline uint32_t *phys_ptr(uint32_t phys_addr) {
    return (uint32_t *) phys_addr;
}

static void zero_frame(uint32_t phys_addr) {
    uint32_t *p = phys_ptr(phys_addr);
    for (uint32_t i = 0; i < ENTRIES_PER_TABLE; i++) {
        p[i] = 0;
    }
}

void paging_init(void) {
    kprintf("Paging: allocating page directory...\n");

    page_directory_phys = pmm_alloc_frame();
    zero_frame(page_directory_phys);
    uint32_t *page_directory = phys_ptr(page_directory_phys);

    kprintf("Paging: page directory at 0x%x. Identity-mapping 0-64MiB (16 page tables)...\n", page_directory_phys);

    for (uint32_t dir_index = 0; dir_index < IDENTITY_MAP_TABLES; dir_index++) {
        uint32_t table_phys = pmm_alloc_frame();
        zero_frame(table_phys);
        uint32_t *table = phys_ptr(table_phys);

        for (uint32_t table_index = 0; table_index < ENTRIES_PER_TABLE; table_index++) {
            uint32_t frame_addr = (dir_index * ENTRIES_PER_TABLE + table_index) * 4096u;
            table[table_index] = frame_addr | PAGE_PRESENT | PAGE_RW;
        }

        page_directory[dir_index] = table_phys | PAGE_PRESENT | PAGE_RW;
    }

    kprintf("Paging: identity map built. Loading CR3 and setting CR0.PG...\n");

    __asm__ __volatile__ ("mov %0, %%cr3" : : "r" (page_directory_phys));

    uint32_t cr0;
    __asm__ __volatile__ ("mov %%cr0, %0" : "=r" (cr0));
    cr0 |= 0x80000000u;
    __asm__ __volatile__ ("mov %0, %%cr0" : : "r" (cr0));

    uint32_t cr0_readback;
    __asm__ __volatile__ ("mov %%cr0, %0" : "=r" (cr0_readback));

    kprintf("Paging: CR0 = 0x%x (PG bit: %u). Paging is now live.\n", cr0_readback, (cr0_readback >> 31) & 1u);
}

/* Generalizes what earlier chapters' own paging_map_page() did to any
 * directory, not only whichever one CR3 currently holds. Everything
 * this function touches -- the directory itself, and every page table
 * it walks or allocates -- is reached through phys_ptr(), a plain
 * physical-address dereference, never through the vaddr being mapped
 * or through whatever CR3 happens to be active right now. That is
 * what makes it safe to build a brand-new process's own page tables
 * here, completely by hand, before that process's own directory is
 * ever loaded into CR3 at all -- this book's kernel never needed a
 * "recursive mapping" trick or similar to edit an inactive directory,
 * because its own physical-equals-virtual invariant already gives it
 * one for free. */
void paging_map_page_in(uint32_t dir_phys, uint32_t vaddr, uint32_t paddr, uint32_t flags) {
    uint32_t dir_index = vaddr >> 22;
    uint32_t table_index = (vaddr >> 12) & 0x3FFu;

    uint32_t *page_directory = phys_ptr(dir_phys);

    uint32_t table_phys;
    if ((page_directory[dir_index] & PAGE_PRESENT) == 0) {
        table_phys = pmm_alloc_frame();
        zero_frame(table_phys);
        page_directory[dir_index] = table_phys | PAGE_PRESENT | PAGE_RW;
    } else {
        table_phys = page_directory[dir_index] & 0xFFFFF000u;
    }

    /* Real x86 paging checks the U/S bit of BOTH the page-directory
     * entry AND the page-table entry -- whichever of the two is more
     * restrictive wins. Setting PAGE_USER only on the PTE below is
     * not enough if this page's own PDE is still supervisor-only,
     * which every identity-mapped PDE genuinely is: paging_init()
     * built them all with just PAGE_PRESENT | PAGE_RW, long before
     * PAGE_USER existed. So whenever this call is meant to grant
     * ring-3 access, the PDE has to be OR'd in too -- Chapter 15's
     * own real captured #PF, caught before this exact fix went in. */
    if (flags & PAGE_USER) {
        page_directory[dir_index] |= PAGE_USER;
    }

    uint32_t *table = phys_ptr(table_phys);
    table[table_index] = (paddr & 0xFFFFF000u) | flags;

    /* Harmless, not merely safe, when `dir_phys` is not the currently
     * active directory: invlpg only ever discards a stale TLB entry
     * for this exact virtual address in the CURRENTLY active address
     * space, which either does not have one yet (a genuine no-op) or,
     * on the rare chance it does, is stale anyway and due for eviction
     * regardless of which directory this call is really building. */
    __asm__ __volatile__ ("invlpg (%0)" : : "r" (vaddr) : "memory");
}

void paging_map_page(uint32_t vaddr, uint32_t paddr, uint32_t flags) {
    paging_map_page_in(page_directory_phys, vaddr, paddr, flags);
}

uint32_t paging_new_address_space(void) {
    uint32_t new_dir_phys = pmm_alloc_frame();
    zero_frame(new_dir_phys);

    uint32_t *new_dir = phys_ptr(new_dir_phys);
    uint32_t *kernel_dir = phys_ptr(page_directory_phys);

    /* Copies all 1024 real page-directory entries, not just the 16
     * paging_init() itself builds -- by the time this function is
     * ever called (Chapter 17's own task_create_process(), after this
     * task's kernel stack has already been kmalloc()'d), the kernel's
     * own directory may already hold real entries paging_init() never
     * built: 020_kmain.c's own 0xC0000000 demo mapping, and however
     * far the kheap (020_kheap.c) has grown by this point. Each entry
     * copied here is a physical page-table ADDRESS, not the table's
     * own contents -- so this new directory shares the exact same
     * underlying page tables as the kernel's own for every one of
     * those entries, and stays correct even if the kheap grows again
     * later, as long as that growth lands inside an ALREADY-shared
     * table rather than requiring a brand-new one. A future chapter
     * that needed the kheap to grow into an entirely fresh directory
     * entry after processes already existed would need to revisit
     * this -- an honest, real limit of this chapter's own design, not
     * one this book's own real run below ever actually hits. */
    for (uint32_t i = 0; i < PAGE_DIR_ENTRIES; i++) {
        new_dir[i] = kernel_dir[i];
    }

    return new_dir_phys;
}

uint32_t paging_translate_in(uint32_t dir_phys, uint32_t vaddr) {
    uint32_t dir_index = vaddr >> 22;
    uint32_t table_index = (vaddr >> 12) & 0x3FFu;

    uint32_t *page_directory = phys_ptr(dir_phys);
    if ((page_directory[dir_index] & PAGE_PRESENT) == 0) {
        return 0;
    }

    uint32_t table_phys = page_directory[dir_index] & 0xFFFFF000u;
    uint32_t *table = phys_ptr(table_phys);
    if ((table[table_index] & PAGE_PRESENT) == 0) {
        return 0;
    }

    return (table[table_index] & 0xFFFFF000u) | (vaddr & 0xFFFu);
}
