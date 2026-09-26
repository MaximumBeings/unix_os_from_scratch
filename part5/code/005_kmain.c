/* Chapter 5: kmain installs the GDT and IDT (Chapters 3 and 4,
 * unchanged), then for the first time in this book's history, actually
 * enables interrupts. Getting there safely takes a specific order:
 * remap the PIC before anything else can fire through it; mask every
 * IRQ line so none of the 14 this kernel still has no handler for can
 * surprise it; unmask exactly the one line (IRQ1) this chapter has a
 * real handler for; install that handler in the IDT; and only then
 * execute STI. After that, kmain has nothing left to do itself -- real
 * work now happens entirely inside irq1_handler, driven by real
 * hardware events, so kmain's own final loop just halts the CPU
 * between interrupts rather than spinning. */

#include "005_gdt.h"
#include "005_idt.h"
#include "005_keyboard.h"
#include "005_pic.h"
#include "005_printf.h"
#include "005_serial.h"
#include "005_vga.h"

void kmain(void) {
    serial_init();
    vga_init();
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);

    gdt_init();
    idt_init();

    pic_remap(0x20, 0x28);
    pic_disable_all();
    keyboard_init();

    kprintf("Unix OS from Scratch -- Chapter 5: kernel entry reached\n");
    kprintf("GDT + IDT loaded; PIC remapped to 0x20/0x28; IRQ1 (keyboard) unmasked\n");
    kprintf("Enabling interrupts now -- type on the real (emulated) keyboard:\n");
    kprintf("> ");

    __asm__ volatile ("sti");

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
