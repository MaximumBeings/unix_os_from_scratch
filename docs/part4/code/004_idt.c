/* Chapter 4: this book's own Interrupt Descriptor Table (IDT). Chapter 1
 * already made one thing about this kernel's machine state explicit:
 * GRUB hands off with interrupts disabled. That has been quietly true
 * every chapter since -- nothing before this one could possibly survive
 * an interrupt or a CPU exception, because there was no table telling
 * the CPU what to do about one. A divide-by-zero, an invalid opcode, a
 * page fault later in this book: every one of those is the CPU looking
 * up an entry in this exact table and jumping to whatever address it
 * finds there. With no IDT installed, or an entry marked not-present,
 * the CPU's response to a fault it cannot dispatch is a triple fault --
 * an immediate, silent reset, with no diagnostic of any kind. This
 * chapter gives the CPU somewhere real to go instead, for exactly one
 * vector: #DE, the Divide Error exception. */

#include <stdint.h>

#include "004_idt.h"

/* One 8-byte IDT entry, packed exactly as the hardware reads it (OSDev
 * Wiki, "Interrupt Descriptor Table"): the 32-bit handler address split
 * into a low and high half, a GDT segment selector, one reserved byte,
 * and one type-attributes byte. */
struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  type_attributes;
    uint16_t offset_high;
} __attribute__((packed));

/* The structure LIDT loads -- the same 48-bit (16-bit limit, 32-bit
 * base) shape as Chapter 3's own gdt_ptr, for the same reason: it is
 * how every one of this CPU's descriptor-table registers is loaded. */
struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

#define IDT_ENTRY_COUNT 256
static struct idt_entry idt_entries[IDT_ENTRY_COUNT];
static struct idt_ptr   idt_pointer;

/* Implemented in 004_isr0.asm: the real machine code the CPU jumps to
 * on a #DE fault. See that file for why this cannot be an ordinary C
 * function. */
extern void isr0(void);

/* Implemented in 004_idt_flush.asm: LIDT, and nothing else -- unlike
 * Chapter 3's gdt_flush, no segment registers need reloading here, and
 * no far jump either. CS is never affected by which IDT is loaded; it
 * is only consulted later, at the moment a real interrupt or exception
 * actually fires, to decide whether the CPU is allowed to take it. */
extern void idt_flush(uint32_t idt_ptr_addr);

static void idt_set_gate(int vector, uint32_t handler, uint16_t selector, uint8_t type_attributes) {
    idt_entries[vector].offset_low      = (uint16_t) (handler & 0xFFFF);
    idt_entries[vector].offset_high     = (uint16_t) ((handler >> 16) & 0xFFFF);
    idt_entries[vector].selector        = selector;
    idt_entries[vector].zero            = 0;
    idt_entries[vector].type_attributes = type_attributes;
}

void idt_init(void) {
    /* Zero every one of the 256 possible vectors first. A zeroed entry
     * has its Present bit clear, which is exactly what this kernel
     * wants for every vector it has not written a real handler for:
     * an honest "not present" rather than a stale or garbage address
     * the CPU might otherwise jump to. */
    for (int i = 0; i < IDT_ENTRY_COUNT; i++) {
        idt_set_gate(i, 0, 0, 0);
    }

    /* Vector 0: #DE, Divide Error -- "DIV and IDIV instructions," no
     * error code pushed (OSDev Wiki, "Interrupt Descriptor Table",
     * exception table). Selector 0x08 is this kernel's own kernel-code
     * descriptor from Chapter 3's GDT (index 1, 1 * 8 = 0x08) -- the
     * segment the CPU switches to while running this handler. The
     * type-attributes byte is cited directly:
     *   "p=1, dpl=0b00, type=0b1110 => type_attributes=0b1000_1110=0x8E"
     * for a 32-bit interrupt gate at ring 0.
     * (OSDev Wiki, "Interrupt Descriptor Table":
     *  https://wiki.osdev.org/Interrupt_Descriptor_Table) */
    idt_set_gate(0, (uint32_t) isr0, 0x08, 0x8E);

    idt_pointer.limit = (uint16_t) (sizeof(idt_entries) - 1);
    idt_pointer.base  = (uint32_t) &idt_entries;

    idt_flush((uint32_t) &idt_pointer);
}
