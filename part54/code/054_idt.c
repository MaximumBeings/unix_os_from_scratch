/* This book's Interrupt Descriptor Table. Vectors 0, 13, 14, 0x20,
 * 0x21, and 0x80 are all unchanged since Chapter 15/25/26 -- see
 * Chapters 4-15 for their own citations. Vector 0x2B (IRQ11) is no
 * longer installed here at all -- this chapter's own real fix to the
 * exact gap Chapter 26's own top-of-file comment named: 054_rtl8139.c
 * now installs that gate itself, at runtime, via this file's own new
 * `idt_install_gate()`, once it has actually discovered which real
 * IRQ line its device landed on, rather than `idt_init()` wiring a
 * fixed vector at compile time and the driver merely hoping the real
 * hardware agrees. */

#include <stdint.h>

#include "054_idt.h"

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

extern void isr0(void);     /* 054_isr0.asm -- unchanged from Chapter 4 */
extern void isr13(void);    /* 054_isr13.asm -- unchanged from Chapter 25 */
extern void isr14(void);    /* 054_isr14.asm -- unchanged from Chapter 10 */
extern void isr128(void);   /* 054_isr128.asm -- unchanged from Chapter 25 */
extern void irq0(void);     /* 054_irq0.asm -- unchanged from Chapter 6 */
extern void irq1(void);     /* 054_irq1.asm -- unchanged from Chapter 5 */

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

    /* Vector 13: #GP, unchanged from Chapter 25. Same 0x08 selector
     * and 0x8E type-attributes byte as every other CPU-raised
     * exception in this book -- DPL=0 here says nothing about which
     * ring can TRIGGER a #GP (the CPU itself always can, from any
     * ring); it only governs whether ring-3 code could deliberately
     * invoke this vector with a bare `int 13` instruction, which
     * nothing in this book ever needs to do. */
    idt_set_gate(13, (uint32_t) isr13, 0x08, 0x8E);

    /* Vector 14: #PF, unchanged from Chapter 10. */
    idt_set_gate(14, (uint32_t) isr14, 0x08, 0x8E);

    /* Vector 0x20: IRQ0, the PIT timer, unchanged from Chapter 6. */
    idt_set_gate(0x20, (uint32_t) irq0, 0x08, 0x8E);

    /* Vector 0x21: IRQ1, the keyboard, unchanged from Chapter 5. */
    idt_set_gate(0x21, (uint32_t) irq1, 0x08, 0x8E);

    /* Vector 0x80: this book's own syscall gate, unchanged from
     * Chapter 25, DPL=3 instead of the 0x8E every earlier gate in this
     * book uses. Byte layout, same P/S/type bits as 0x8E, but DPL
     * (bits 6-5) = 11 instead of 00: 1 11 0 1110 = 0xEE. The CPU checks
     * a software INT's own target gate DPL against the CALLER's CPL
     * (int 0x80 is only ever executed from ring 3 in this book) and
     * requires CPL <= gate DPL -- the opposite direction from every
     * other privilege check in this book, and the one and only reason
     * this particular gate needs anything other than 0x8E at all. */
    idt_set_gate(0x80, (uint32_t) isr128, 0x08, 0xEE);

    idt_pointer.limit = (uint16_t) (sizeof(idt_entries) - 1);
    idt_pointer.base  = (uint32_t) &idt_entries;

    idt_flush((uint32_t) &idt_pointer);
}

/* Reads the real EFLAGS.IF bit (bit 9) without disturbing it -- the
 * same real "was this interrupt flag already set" question
 * `idt_install_gate()`/`idt_uninstall_gate()` need before they `cli`,
 * so they restore it afterward rather than unconditionally `sti`-ing
 * a caller that genuinely had interrupts disabled on purpose. */
static int interrupts_were_enabled(void) {
    uint32_t eflags;
    __asm__ volatile ("pushfl; popl %0" : "=r" (eflags));
    return (eflags & (1u << 9)) != 0;
}

void idt_install_gate(int vector, uint32_t handler, uint16_t selector, uint8_t type_attributes) {
    int was_enabled = interrupts_were_enabled();
    __asm__ volatile ("cli");
    idt_set_gate(vector, handler, selector, type_attributes);
    if (was_enabled) {
        __asm__ volatile ("sti");
    }
}

void idt_uninstall_gate(int vector) {
    int was_enabled = interrupts_were_enabled();
    __asm__ volatile ("cli");
    idt_set_gate(vector, 0, 0, 0);
    if (was_enabled) {
        __asm__ volatile ("sti");
    }
}

int idt_gate_is_present(int vector) {
    return (idt_entries[vector].type_attributes & 0x80u) != 0u;
}
