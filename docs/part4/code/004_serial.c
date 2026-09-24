/* The COM1 UART driver from Chapter 1, moved into its own file and given
 * a real header now that more than one file needs it -- unchanged in
 * behavior from Chapters 1 and 2's own private copies. */

#include "004_serial.h"

static inline void outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

#define COM1 0x3F8

void serial_init(void) {
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

void serial_putc(char c) {
    while (!serial_transmit_empty()) { }
    outb(COM1, (unsigned char) c);
}

void serial_puts(const char *s) {
    for (; *s; s++) serial_putc(*s);
}
