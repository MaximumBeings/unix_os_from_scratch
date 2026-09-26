/* Chapter 4: a real VGA text-mode driver. Unlike Chapter 1's COM1 UART,
 * this hardware needs no port I/O to draw a character -- physical address
 * 0xB8000 IS the screen, an 80x25 grid of 2-byte cells (a character byte
 * and a colour attribute byte), writable exactly like any other memory
 * this kernel can already reach directly (no paging exists yet, so
 * physical and linear addresses are still the same thing). Moving the
 * blinking hardware cursor is the one part of this driver that still
 * needs real port I/O, through the VGA CRT controller's own index/data
 * port pair. */

#include <stddef.h>
#include <stdint.h>

#include "004_vga.h"

static inline void outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((uint16_t *) 0xB8000)

/* Real, cited CRTC register ports (OSDev Wiki, "Text Mode Cursor"). */
#define VGA_CRTC_INDEX_PORT  0x3D4
#define VGA_CRTC_DATA_PORT   0x3D5
#define VGA_CRTC_CURSOR_HIGH 0x0E
#define VGA_CRTC_CURSOR_LOW  0x0F

/* Real, cited attribute-byte layout (OSDev Wiki, "Text mode"): bits 0-2
 * are the foreground colour, bit 3 is the foreground "bright" bit, bits
 * 4-6 are the background colour, bit 7 enables blink (mode-dependent) --
 * i.e. the full 4-bit background nibble shifted up four bits, OR'd with
 * the full 4-bit foreground nibble. */
static inline uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) {
    return (uint8_t) fg | (uint8_t) (bg << 4);
}

/* One VGA text-mode cell packed into 16 bits. Whether the character or
 * the colour byte lands at the LOWER of the two memory addresses is
 * exactly what this chapter's own worked example verifies directly
 * against a real QEMU memory dump, rather than assuming it. */
static inline uint16_t vga_entry(unsigned char c, uint8_t color) {
    return (uint16_t) c | (uint16_t) color << 8;
}

static size_t vga_row;
static size_t vga_col;
static uint8_t vga_color_attr;
/* volatile: 0xB8000 is memory-mapped hardware, not ordinary RAM. Without
 * this, an optimizing compiler is free to treat writes to vga_buffer[i]
 * as dead stores whenever a later write to the same index looks
 * redundant from its own point of view (e.g. two vga_init() calls back
 * to back, or a scroll immediately followed by more writes at the same
 * cell) -- perfectly legal for ordinary memory, silently wrong for a
 * framebuffer the screen itself reads from continuously. */
static volatile uint16_t *vga_buffer;

static void vga_set_cursor(size_t x, size_t y) {
    uint16_t pos = (uint16_t) (y * VGA_WIDTH + x);
    outb(VGA_CRTC_INDEX_PORT, VGA_CRTC_CURSOR_LOW);
    outb(VGA_CRTC_DATA_PORT, (uint8_t) (pos & 0xFF));
    outb(VGA_CRTC_INDEX_PORT, VGA_CRTC_CURSOR_HIGH);
    outb(VGA_CRTC_DATA_PORT, (uint8_t) ((pos >> 8) & 0xFF));
}

static void vga_scroll(void) {
    for (size_t y = 1; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[(y - 1) * VGA_WIDTH + x] = vga_buffer[y * VGA_WIDTH + x];
        }
    }
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', vga_color_attr);
    }
    vga_row = VGA_HEIGHT - 1;
}

void vga_init(void) {
    vga_buffer = VGA_MEMORY;
    vga_color_attr = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = vga_entry(' ', vga_color_attr);
        }
    }
    vga_row = 0;
    vga_col = 0;
    vga_set_cursor(vga_col, vga_row);
}

void vga_set_color(enum vga_color fg, enum vga_color bg) {
    vga_color_attr = vga_entry_color(fg, bg);
}

void vga_putc(char c) {
    if (c == '\n') {
        vga_col = 0;
        vga_row++;
    } else {
        vga_buffer[vga_row * VGA_WIDTH + vga_col] = vga_entry((unsigned char) c, vga_color_attr);
        vga_col++;
        if (vga_col == VGA_WIDTH) {
            vga_col = 0;
            vga_row++;
        }
    }
    if (vga_row == VGA_HEIGHT) {
        vga_scroll();
    }
    vga_set_cursor(vga_col, vga_row);
}

void vga_puts(const char *s) {
    for (; *s; s++) vga_putc(*s);
}
