/* Chapter 2: kmain now drives two real, independent output devices -- the
 * COM1 UART from Chapter 1 (kept so this chapter's own locked output
 * stays exact, capturable text) and the VGA text-mode framebuffer this
 * chapter adds. Unlike Chapter 1, this kmain does not use QEMU's
 * isa-debug-exit device: it deliberately halts forever (falling through
 * to 002_boot.asm's own cli/hlt/jmp .hang loop) so this chapter's own
 * verification -- a real QEMU monitor session, connected from OUTSIDE
 * the machine while it is still running -- has time to examine the VGA
 * framebuffer's actual memory contents before the monitor's own "quit"
 * command ends the machine itself. */

#include "002_vga.h"

static inline void outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

#define COM1 0x3F8

static void serial_init(void) {
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}

static int serial_transmit_empty(void) {
    return inb(COM1 + 5) & 0x20;
}

static void serial_putc(char c) {
    while (!serial_transmit_empty()) { }
    outb(COM1, (unsigned char)c);
}

static void serial_puts(const char *s) {
    for (; *s; s++) serial_putc(*s);
}

void kmain(void) {
    serial_init();
    serial_puts("Unix OS from Scratch -- Chapter 2: kernel entry reached\n");

    vga_init();
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_puts("Unix OS from Scratch -- Chapter 2: a real VGA text-mode driver\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_puts("This line is printed in the default color.\n");

    serial_puts("VGA driver ran; halting now for external memory inspection\n");
}
