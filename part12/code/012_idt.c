/* This book's Interrupt Descriptor Table, extended from Chapter 6. The
 * struct layout, the type-attributes byte derivation, and every gate
 * through vector 0x21 are unchanged -- see Chapters 4-6 for the full
 * citation and derivation of those. This chapter adds exactly one
 * more real gate: vector 14, #PF -- the page fault exception, live
 * and capable of firing since Chapter 8 turned paging on, but with no
 * real handler installed for it until now. */

#include <stdint.h>

#include "012_idt.h"

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  type_attributes;
    uint16_t offset_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

#define IDT_ENTRY_COUNT 256
static struct idt_entry idt_entries[IDT_ENTRY_COUNT];
static struct idt_ptr   idt_pointer;

extern void isr0(void);    /* 012_isr0.asm -- unchanged from Chapter 4 */
extern void isr14(void);   /* 012_isr14.asm -- this chapter's own stub */
extern void irq0(void);    /* 012_irq0.asm -- unchanged from Chapter 6 */
extern void irq1(void);    /* 012_irq1.asm -- unchanged from Chapter 5 */

extern void idt_flush(uint32_t idt_ptr_addr);

static void idt_set_gate(int vector, uint32_t handler, uint16_t selector, uint8_t type_attributes) {
    idt_entries[vector].offset_low      = (uint16_t) (handler & 0xFFFF);
    idt_entries[vector].offset_high     = (uint16_t) ((handler >> 16) & 0xFFFF);
    idt_entries[vector].selector        = selector;
    idt_entries[vector].zero            = 0;
    idt_entries[vector].type_attributes = type_attributes;
}

void idt_init(void) {
    for (int i = 0; i < IDT_ENTRY_COUNT; i++) {
        idt_set_gate(i, 0, 0, 0);
    }

    /* Vector 0: #DE, unchanged from Chapter 4. */
    idt_set_gate(0, (uint32_t) isr0, 0x08, 0x8E);

    /* Vector 14: #PF, this chapter's own new gate. Same selector
     * (0x08) and 0x8E type-attributes byte as every other gate here --
     * the IDT descriptor itself does not know or care that this
     * particular vector's exception pushes an error code; only
     * 012_isr14.asm's own stub has to know that. */
    idt_set_gate(14, (uint32_t) isr14, 0x08, 0x8E);

    /* Vector 0x20: IRQ0, the PIT timer, unchanged from Chapter 6. */
    idt_set_gate(0x20, (uint32_t) irq0, 0x08, 0x8E);

    /* Vector 0x21: IRQ1, the keyboard, unchanged from Chapter 5. */
    idt_set_gate(0x21, (uint32_t) irq1, 0x08, 0x8E);

    idt_pointer.limit = (uint16_t) (sizeof(idt_entries) - 1);
    idt_pointer.base  = (uint32_t) &idt_entries;

    idt_flush((uint32_t) &idt_pointer);
}
