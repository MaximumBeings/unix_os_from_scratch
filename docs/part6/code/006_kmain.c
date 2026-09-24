/* Chapter 6: kmain installs the GDT and IDT (Chapters 3 and 4,
 * unchanged), remaps and masks the PIC, then unmasks two real IRQ
 * lines this time instead of one -- IRQ1 (the keyboard, unchanged from
 * Chapter 5) and this chapter's own IRQ0 (the PIT timer). Order still
 * matters exactly as it did in Chapter 5: remap the PIC before
 * anything else can fire through it; mask every line; unmask only the
 * lines this kernel actually has handlers for; and only then execute
 * STI. After that, kmain has nothing left to do itself -- real work
 * now happens entirely inside irq0_handler and irq1_handler, driven by
 * real hardware events, so kmain's own final loop just halts the CPU
 * between interrupts rather than spinning. */

#include "006_gdt.h"
#include "006_idt.h"
#include "006_keyboard.h"
#include "006_pic.h"
#include "006_pit.h"
#include "006_printf.h"
#include "006_serial.h"
#include "006_vga.h"

#define TIMER_FREQUENCY_HZ 100

void kmain(void) {
    serial_init();
    vga_init();
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);

    gdt_init();
    idt_init();

    pic_remap(0x20, 0x28);
    pic_disable_all();
    keyboard_init();
    pit_init(TIMER_FREQUENCY_HZ);

    kprintf("Unix OS from Scratch -- Chapter 6: kernel entry reached\n");
    kprintf("GDT + IDT loaded; PIC remapped to 0x20/0x28\n");
    kprintf("IRQ0 (PIT timer, %u Hz) and IRQ1 (keyboard) unmasked\n", TIMER_FREQUENCY_HZ);
    kprintf("Enabling interrupts now -- a real tick print appears once a second:\n");

    __asm__ volatile ("sti");

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
