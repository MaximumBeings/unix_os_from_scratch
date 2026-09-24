/* Chapter 3: kmain now installs this book's own GDT before doing
 * anything else, then proves it survived by continuing to run at all --
 * a bad GDT install does not print an error, it triple-faults the
 * machine immediately. Everything after that uses kprintf instead of
 * raw serial_puts/vga_puts calls, this chapter's own real formatted
 * output running with no libc underneath it. Like Chapter 2, this
 * kmain deliberately never exits: it falls through to 003_boot.asm's
 * own halt loop so a QEMU monitor session, connected from outside,
 * still has a live machine to screendump. */

#include "003_gdt.h"
#include "003_printf.h"
#include "003_serial.h"
#include "003_vga.h"

void kmain(void) {
    serial_init();
    vga_init();
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);

    gdt_init();

    kprintf("Unix OS from Scratch -- Chapter 3: kernel entry reached\n");
    kprintf("GDT loaded: 3 descriptors (null, kernel code 0x08, kernel data 0x10)\n");
    kprintf("kprintf self-test -- int: %d  negative: %d  hex: %x  char: %c  str: %s  percent: %%\n",
            42, -7, 0x1abc, 'Z', "unix_os_from_scratch");
}
