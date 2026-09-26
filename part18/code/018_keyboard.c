/* Chapter 5: this book's first real input device. Every prior chapter
 * only ever produced output; this one reads real scancodes off the
 * PS/2 keyboard controller's data port and turns them into characters
 * this kernel can print right back out through kprintf. */

#include <stdint.h>

#include "018_keyboard.h"
#include "018_pic.h"
#include "018_printf.h"

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* "The Data Port (typically IO Port 0x60) is used for reading data
 * that was received from a PS/2 device" (OSDev Wiki, "'8042' PS/2
 * Controller": https://wiki.osdev.org/%228042%22_PS/2_Controller).
 * "The interrupt used by keyboard is typically ISA interrupt 1" --
 * IRQ1, this chapter's own gate at IDT vector 0x21 after the PIC
 * remap. */
#define KBD_DATA_PORT 0x60

/* A real, cited subset of Scan Code Set 1's make codes (OSDev Wiki,
 * "PS/2 Keyboard": https://wiki.osdev.org/PS/2_Keyboard) -- every
 * letter key, space, and Enter, enough to type real words back out
 * through kprintf. Anything this table leaves at 0 (Shift, Ctrl, the
 * arrow keys, digits, punctuation, and so on) is a deliberate scope
 * limit, not an oversight: this chapter has no shift-state tracking
 * and no keymap beyond lowercase letters, which is enough to prove a
 * real hardware interrupt genuinely delivered real keystrokes. A later
 * chapter can grow this table without touching anything else in this
 * file. */
static const char scancode_to_ascii[128] = {
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r', [0x14] = 't', [0x15] = 'y',
    [0x16] = 'u', [0x17] = 'i', [0x18] = 'o', [0x19] = 'p',
    [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f', [0x22] = 'g', [0x23] = 'h',
    [0x24] = 'j', [0x25] = 'k', [0x26] = 'l',
    [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v', [0x30] = 'b', [0x31] = 'n', [0x32] = 'm',
    [0x39] = ' ',
    [0x1C] = '\n',
};

/* Called from 018_irq1.asm's own stub on every real IRQ1. The
 * controller reports both a key-down ("make code") and a key-up
 * ("break code") for every physical keypress -- OSDev's own scan-code
 * table lists, for example, 0x1E for "A pressed" and 0x9E for "A
 * released", and 0x9E - 0x1E = 0x80 exactly, for every letter key the
 * same page lists: the break code is always the make code with the
 * high bit set. This handler only reacts to make codes (bit 7 clear);
 * break codes are read (the port must always be read, or the
 * controller will not deliver its next byte) and then silently
 * discarded, since this chapter never needs to know when a key was
 * released. */
void irq1_handler(void) {
    uint8_t scancode = inb(KBD_DATA_PORT);

    if (!(scancode & 0x80)) {
        char c = scancode_to_ascii[scancode];
        if (c != 0) {
            kprintf("%c", c);
        }
    }

    pic_send_eoi(1);
}

void keyboard_init(void) {
    pic_clear_mask(1);
}
