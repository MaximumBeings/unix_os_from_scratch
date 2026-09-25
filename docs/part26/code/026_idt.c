/* This book's Interrupt Descriptor Table. Vectors 0, 13, 14, 0x20,
 * 0x21, and 0x80 are all unchanged since Chapter 15/25 -- see Chapters
 * 4-15 for their own citations. This chapter adds exactly one new
 * gate: vector 0x2B, this book's first-ever gate for a SLAVE-PIC line
 * (IRQ 8-15) rather than the master PIC's own IRQ0/IRQ1 -- see
 * 026_rtl8139.h's own top-of-file comment for the real reasoning
 * behind that exact vector number, and 026_pic.c's own real IRQ2
 * cascade-unmask requirement that has to hold before this gate can
 * ever actually fire. Installed with the same 0x08 selector and 0x8E
 * type-attributes byte as every other hardware-IRQ gate in this book
 * (IRQ0/IRQ1) -- nothing about a slave-PIC line changes the gate's own
 * byte layout, only which real 8259 chip has to be told about it at
 * EOI time (026_pic.c's own pic_send_eoi(), unchanged since Chapter
 * 5). */

#include <stdint.h>

#include "026_idt.h"

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

extern void isr0(void);     /* 026_isr0.asm -- unchanged from Chapter 4 */
extern void isr13(void);    /* 026_isr13.asm -- this chapter's own stub */
extern void isr14(void);    /* 026_isr14.asm -- unchanged from Chapter 10 */
extern void isr128(void);   /* 026_isr128.asm -- this chapter's own stub */
extern void irq0(void);     /* 026_irq0.asm -- unchanged from Chapter 6 */
extern void irq1(void);     /* 026_irq1.asm -- unchanged from Chapter 5 */
extern void irq11(void);    /* 026_irq11.asm -- this chapter's own new stub */

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

    /* Vector 13: #GP, this chapter's own new gate. Same 0x08 selector
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

    /* Vector 0x2B: IRQ11, this chapter's own new RTL8139 gate --
     * 026_rtl8139.c's own real, live PCI Interrupt Line read (offset
     * 0x3C) verifies the real hardware actually agrees this is where
     * its interrupts land, refusing rather than silently trusting this
     * fixed vector if it ever doesn't. Present (installed) here, at
     * boot, like every other gate in this book; actually UNMASKED at
     * the 8259 PIC level only once rtl8139_init() itself runs and
     * succeeds -- the same "install the gate at boot, unmask the line
     * from the owning driver's own init function" split this book has
     * followed since Chapter 6/pit_init()'s own pic_clear_mask(0). */
    idt_set_gate(0x2B, (uint32_t) irq11, 0x08, 0x8E);

    /* Vector 0x80: this chapter's own syscall gate, DPL=3 instead of
     * the 0x8E every earlier gate in this book uses. Byte layout,
     * same P/S/type bits as 0x8E, but DPL (bits 6-5) = 11 instead of
     * 00: 1 11 0 1110 = 0xEE. The CPU checks a software INT's own
     * target gate DPL against the CALLER's CPL (int 0x80 is only
     * ever executed from ring 3 in this chapter) and requires
     * CPL <= gate DPL -- the opposite direction from every other
     * privilege check in this book, and the one and only reason this
     * particular gate needs anything other than 0x8E at all. */
    idt_set_gate(0x80, (uint32_t) isr128, 0x08, 0xEE);

    idt_pointer.limit = (uint16_t) (sizeof(idt_entries) - 1);
    idt_pointer.base  = (uint32_t) &idt_entries;

    idt_flush((uint32_t) &idt_pointer);
}
