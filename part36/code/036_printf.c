/* Chapter 3's own kprintf, extended for the first time since it was
 * written. Everything through %s and %% is unchanged; this chapter adds
 * exactly one new conversion, %llx, and for a concrete, real reason: the
 * Multiboot2 memory map this chapter reads genuinely stores addr/len as
 * 64-bit fields (GNU Multiboot2 Specification, "Boot information
 * format"), and this kernel is still a 32-bit build -- va_arg(args, int)
 * would read only half of a real 64-bit argument off the stack, and
 * every argument after it would then be misaligned too. */

#include <stdarg.h>

#include "036_printf.h"
#include "036_serial.h"
#include "036_vga.h"

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

/* Hexadecimal only, deliberately, and not just because %llx is the only
 * 64-bit conversion this kernel supports: this freestanding build links
 * against no libgcc, and a real 64-bit division or modulo on a 32-bit
 * target compiles down to calls to compiler-runtime helpers
 * (__udivdi3/__umoddi3) that simply do not exist in this link -- the
 * first build of this file failed with exactly those two undefined
 * references. Hex digits are nibbles, so extracting them with `& 0xF`
 * and `>>= 4` needs nothing but a 64-bit shift and mask, both of which
 * GCC emits as ordinary inline SHRD/SHR instruction pairs on i386, not a
 * runtime call -- avoiding the dependency entirely rather than adding
 * one, which is why this function has no base parameter the way
 * kprint_uint above does. */
static void kprint_hex64(unsigned long long value, int uppercase) {
    char buf[16];
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    int i = 0;

    if (value == 0) {
        buf[i++] = '0';
    } else {
        while (value > 0) {
            buf[i++] = digits[value & 0xFu];
            value >>= 4;
        }
    }
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
            case 'l':
                /* Only one real two-character extension is recognized:
                 * "ll" followed by 'x'. Anything else falls through to
                 * the same literal-echo behavior as an unrecognized
                 * specifier, on purpose -- this kernel has no use yet
                 * for %ld/%lu, so it does not pretend to support them. */
                if (*(p + 1) == 'l' && *(p + 2) == 'x') {
                    p += 2;
                    kprint_hex64(va_arg(args, unsigned long long), 0);
                } else {
                    kputc('%');
                    kputc(*p);
                }
                break;
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
