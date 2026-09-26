#include <stdint.h>

#include "035_tss.h"

/* The CPU's real 32-bit TSS layout (cited field-for-field from the
 * OSDev Wiki's own "Task State Segment" page): a fixed 104-byte
 * structure with two fields this chapter actually cares about --
 * ESP0 at offset 0x04 ("The Stack Pointer used to load the stack
 * when a privilege level change occurs from a lower privilege level
 * to a higher one") and SS0 at offset 0x08 ("The Segment Selector
 * used to load the stack when a privilege level change occurs from a
 * lower privilege level to a higher one") -- plus a real IOPB
 * (`iomap_base`) field at the very end. Every other field here exists
 * only because LTR requires the CPU's own real, fixed-size structure;
 * this book's software task switching never reads or writes eip,
 * eflags, eax..edi, or any of the saved segment registers below --
 * those only matter to the CPU's OWN hardware task-switch mechanism,
 * which this kernel does not use at all. */
struct tss_entry {
    uint32_t prev_tss;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax, ecx, edx, ebx;
    uint32_t esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} __attribute__((packed));

static struct tss_entry tss;

void tss_init(void) {
    uint8_t *raw = (uint8_t *) &tss;
    for (uint32_t i = 0; i < sizeof(tss); i++) {
        raw[i] = 0;
    }

    /* SS0 = this kernel's own kernel-data selector (GDT index 2,
     * 2 * 8 = 0x10, unchanged since Chapter 5) -- every privilege-
     * elevating interrupt loads SS with exactly this value before
     * this kernel's own ISR stub ever runs a single instruction. */
    tss.ss0 = 0x10;

    /* Setting the I/O map base to this TSS's own total size, rather
     * than a real offset that points AT a real bitmap somewhere
     * inside it, means the CPU always finds the "bitmap" it looks up
     * sitting past this structure's own last byte -- which the CPU
     * treats as "no I/O permission bitmap is present at all," and
     * therefore every single port I/O instruction (IN/OUT/INS/OUTS)
     * issued from ring 3 is forbidden, with no per-port exceptions.
     * This chapter never demonstrates that directly, but it is the
     * same real hardware-privilege mechanism this chapter's own CLI
     * demonstration relies on -- ring 3 code simply cannot touch
     * anything this kernel has not explicitly, deliberately opened up
     * for it. */
    tss.iomap_base = (uint16_t) sizeof(tss);
}

void tss_set_kernel_stack(uint32_t esp0) {
    tss.esp0 = esp0;
}

void tss_load(uint16_t selector) {
    __asm__ volatile ("ltr %0" : : "rm" (selector));
}

const void *tss_get_address(void) {
    return &tss;
}

uint32_t tss_get_size(void) {
    return (uint32_t) sizeof(tss);
}
