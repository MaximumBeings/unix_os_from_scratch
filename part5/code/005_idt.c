/* This book's Interrupt Descriptor Table, extended from Chapter 4. The
 * struct layout, the type-attributes byte derivation, and vector 0's
 * own #DE gate are all unchanged -- see Chapter 4 for the full citation
 * and derivation of those. This chapter adds exactly one more real
 * gate: vector 0x21, IRQ1, the keyboard -- the first hardware interrupt
 * this kernel has ever handled, as opposed to a CPU-generated
 * exception. */

#include <stdint.h>

#include "005_idt.h"

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

extern void isr0(void);   /* 005_isr0.asm -- unchanged from Chapter 4 */
extern void irq1(void);   /* 005_irq1.asm -- this chapter's own stub */

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

    /* Vector 0x21: IRQ1, the keyboard, after 005_pic.c's own PIC_remap
     * moves the master PIC's vector offset to 0x20 -- IRQ0 becomes
     * vector 0x20, IRQ1 becomes 0x21, and so on. Same selector (0x08,
     * this kernel's flat code segment) and the same 0x8E type-
     * attributes byte as vector 0: a hardware interrupt gate is
     * structurally identical to an exception gate in the IDT: the CPU
     * does not distinguish "this vector is a CPU exception" from
     * "this vector is a hardware IRQ" at the descriptor level, only
     * by which vector number ends up being invoked. */
    idt_set_gate(0x21, (uint32_t) irq1, 0x08, 0x8E);

    idt_pointer.limit = (uint16_t) (sizeof(idt_entries) - 1);
    idt_pointer.base  = (uint32_t) &idt_entries;

    idt_flush((uint32_t) &idt_pointer);
}
