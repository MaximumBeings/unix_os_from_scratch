#ifndef UNIX_OS_047_IDT_H
#define UNIX_OS_047_IDT_H

#include <stdint.h>

/* Installs this book's IDT: every gate through vector 0x80, unchanged
 * since Chapter 15/25 -- `idt_init()` itself no longer installs
 * vector 0x2B (IRQ11) at compile time; see `idt_install_gate()`'s own
 * comment below and 047_rtl8139.c's own `rtl8139_init()` for this
 * chapter's own real fix. */
void idt_init(void);

/* This chapter's own new real mechanism, closing the exact gap
 * Chapter 26's own top-of-file comment named and left open: "there is
 * no mechanism anywhere in this book (yet) for installing a NEW gate
 * at runtime once a driver discovers which real IRQ line its device
 * actually landed on." `idt_init()` above still runs once, at boot,
 * and still loads the IDT into the CPU via `idt_flush()` -- but the
 * real x86 architecture itself never requires reloading IDTR (`lidt`)
 * after that: the CPU reads the IDT directly out of memory on every
 * real interrupt, so writing a new entry into the same, already-
 * loaded table is immediately live. `idt_install_gate()` is exactly
 * that write, exposed publicly for the first time -- the same real
 * `idt_set_gate()` logic `idt_init()` has used internally since
 * Chapter 4, just no longer private to this file, and now callable
 * at ANY point after `idt_init()` has run, not only during it.
 *
 * Real hazard, handled honestly rather than ignored: writing a 64-bit
 * IDT entry is not a single atomic store on this real 32-bit
 * architecture, and a real hardware interrupt landing on `vector`
 * mid-write, while only half the new entry has been written, would
 * read a torn, meaningless descriptor. `idt_install_gate()` disables
 * real maskable interrupts (`cli`) for the few real instructions the
 * write itself takes, then restores whatever real interrupt-flag
 * state the caller already had (`sti` only if it was genuinely set
 * beforehand) -- the same real critical-section discipline this book
 * already uses wherever two real concurrent paths could otherwise
 * observe a torn update. */
void idt_install_gate(int vector, uint32_t handler, uint16_t selector, uint8_t type_attributes);

/* Removes a gate previously installed by `idt_install_gate()`,
 * clearing its real present bit (so a genuine interrupt arriving on
 * `vector` afterward would fault rather than run stale code) -- the
 * natural real complement `idt_install_gate()`'s own real, two-way
 * lifecycle needs, even though Chapter 26's own text only ever named
 * "installing" as the missing half. Same real `cli`/`sti` critical-
 * section discipline as `idt_install_gate()`. */
void idt_uninstall_gate(int vector);

/* Returns 1 if `vector`'s own real present bit (bit 7 of the real
 * type-attributes byte, cited the same way every gate installation in
 * this book already is) is currently set, 0 otherwise -- lets this
 * chapter's own demo confirm a real install/uninstall actually took
 * effect by reading the live table back, without needing to trigger a
 * real interrupt just to find out. */
int idt_gate_is_present(int vector);

#endif
