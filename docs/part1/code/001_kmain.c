/* Chapter 1: our first freestanding "kernel". It writes to the COM1 serial
 * port rather than the VGA text buffer so this book can lock its exact,
 * real, captured text output into the page -- no screenshot of a virtual
 * monitor needed, just QEMU's own -serial stdio flag. Everything above
 * main() (the boot stub in 001_boot.asm and this file's own outb/inb pair)
 * exists because, the instant GRUB hands control to _start, there is no
 * C runtime, no stack the compiler can assume, and no OS underneath us to
 * ask for I/O -- kmain() below is the first C function this machine has
 * ever run that WE wrote, and serial_puts() is the first output any part
 * of this book produces without an operating system's help. */

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

/* QEMU's isa-debug-exit device (real, standard OS-dev technique): a write
 * to I/O port 0xf4 makes QEMU itself exit with status (value << 1) | 1,
 * instead of the kernel having to halt forever and an outside process
 * having to kill QEMU to get its output back. Without real hardware
 * behind this port, this instruction is meaningless outside QEMU -- a
 * real kernel bound for real hardware would never do this; it exists here
 * only so this chapter's own locked output ends cleanly and
 * deterministically, without a stray "killed by timeout" line that has
 * nothing to do with the kernel's own behavior. */
#define QEMU_DEBUG_EXIT_PORT 0xf4

static void qemu_exit(unsigned char code) {
    outb(QEMU_DEBUG_EXIT_PORT, code);
}

void kmain(void) {
    serial_init();
    serial_puts("Unix OS from Scratch -- Chapter 1: kernel entry reached\n");
    serial_puts("multiboot2 + nasm + freestanding gcc + ld + qemu pipeline: OK\n");
    qemu_exit(0);
}
