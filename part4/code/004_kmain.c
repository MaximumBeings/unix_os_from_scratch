/* Chapter 4: kmain installs the GDT (Chapter 3, unchanged) and now this
 * chapter's own IDT, then deliberately triggers a real #DE exception --
 * a runtime division by a volatile zero, not a compile-time constant
 * the compiler could fold away or warn about, so the fault genuinely
 * comes from the CPU executing an IDIV instruction at runtime. If the
 * IDT and its one real gate are wired correctly, control lands in
 * 004_isr0.asm's stub and then 004_isr_handlers.c's real handler,
 * which prints and halts -- so the line right after the division is
 * this chapter's own proof that it should *never* print. */

#include "004_gdt.h"
#include "004_idt.h"
#include "004_printf.h"
#include "004_serial.h"
#include "004_vga.h"

void kmain(void) {
    serial_init();
    vga_init();
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);

    gdt_init();
    idt_init();

    kprintf("Unix OS from Scratch -- Chapter 4: kernel entry reached\n");
    kprintf("GDT loaded (Chapter 3); IDT loaded: 256 entries, 1 real gate (vector 0)\n");
    kprintf("Triggering a real divide-by-zero to prove the handler is live...\n");

    volatile int numerator = 42;
    volatile int denominator = 0;
    volatile int result = numerator / denominator;

    kprintf("THIS LINE SHOULD NEVER PRINT -- result was %d\n", result);
}
