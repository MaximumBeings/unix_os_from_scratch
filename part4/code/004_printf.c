/* Chapter 4: this kernel's own printf, built on top of two drivers this
 * book already has -- COM1 serial (Chapter 1) and the VGA framebuffer
 * (Chapter 2) -- rather than on any C library. Freestanding C still
 * gives this file <stdarg.h> for real (it is a compiler-support header,
 * not part of the hosted standard library this kernel deliberately does
 * without), which is what makes a variadic function like this possible
 * at all. */

#include <stdarg.h>

#include "004_printf.h"
#include "004_serial.h"
#include "004_vga.h"

/* Every character this file ever emits goes through here, to both real
 * output devices this book has built so far -- so a single kprintf call
 * shows up identically in the exact, capturable serial log and on the
 * real (emulated) screen. */
static void kputc(char c) {
    serial_putc(c);
    vga_putc(c);
}

static void kprint_uint(unsigned int value, unsigned int base, int uppercase) {
    char buf[32];
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    int i = 0;

    if (value == 0) {
        buf[i++] = '0';
    } else {
        while (value > 0) {
            buf[i++] = digits[value % base];
            value /= base;
        }
    }
    /* Digits were produced least-significant-first; emit them in
     * reverse so they read correctly. */
    while (i > 0) {
        kputc(buf[--i]);
    }
}

static void kprint_int(int value) {
    if (value < 0) {
        kputc('-');
        /* Negate into an unsigned value before printing, so the most
         * negative int (whose magnitude has no positive int
         * representation) still prints correctly. */
        kprint_uint((unsigned int) (-(long long) value), 10, 0);
    } else {
        kprint_uint((unsigned int) value, 10, 0);
    }
}

void kprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            kputc(*p);
            continue;
        }
        p++;
        switch (*p) {
            case 'd':
                kprint_int(va_arg(args, int));
                break;
            case 'u':
                kprint_uint(va_arg(args, unsigned int), 10, 0);
                break;
            case 'x':
                kprint_uint(va_arg(args, unsigned int), 16, 0);
                break;
            case 'c':
                kputc((char) va_arg(args, int));
                break;
            case 's': {
                const char *s = va_arg(args, const char *);
                for (; *s; s++) kputc(*s);
                break;
            }
            case '%':
                kputc('%');
                break;
            case '\0':
                /* A lone trailing '%' with nothing after it: step back
                 * so the for-loop's own p++ lands exactly on the
                 * string's real terminator, instead of reading past it. */
                p--;
                break;
            default:
                kputc('%');
                kputc(*p);
                break;
        }
    }

    va_end(args);
}
