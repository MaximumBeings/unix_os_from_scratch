/* Chapter 6: this book's first source of real, hardware-driven time.
 * Every chapter through Chapter 5 could only react to events -- a CPU
 * exception, a keypress -- with no notion of "how much time has passed"
 * in between. The 8253/8254 Programmable Interval Timer (PIT) fixes
 * that: programmed correctly, it fires IRQ0 at a steady, chosen rate,
 * and this chapter turns those ticks into a real, growing counter this
 * kernel can trust. */

#include <stdint.h>

#include "024_pic.h"
#include "024_pit.h"
#include "024_printf.h"
#include "024_task.h"

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* "Channel 0 is connected directly to IRQ0" and is read/written through
 * IO port 0x40; "the Mode/Command register ... is located at IO port
 * 0x43" (OSDev Wiki, "Programmable Interval Timer":
 * https://wiki.osdev.org/Programmable_Interval_Timer). */
#define PIT_CH0_DATA_PORT 0x40
#define PIT_COMMAND_PORT  0x43

/* The command byte this chapter sends is built from the cited bit
 * layout: bits 7-6 = 00 (select channel 0), bits 5-4 = 11 (access
 * mode: lobyte, then hibyte), bits 3-1 = 011 (Mode 3, square wave
 * generator), bit 0 = 0 (16-bit binary, not BCD). 0b00110110 = 0x36. */
#define PIT_CH0_MODE3_BINARY 0x36

/* "The oscillator used by the PIT chip runs at (roughly) 1.193182 MHz"
 * -- the same cited page's own reload-value formula is
 * reload_value = 1193182 / desired_frequency_Hz. */
#define PIT_BASE_FREQUENCY_HZ 1193182u

static volatile uint32_t tick_count = 0;

/* At 100 Hz (this chapter's chosen rate) this prints once per real
 * second -- a deliberately coarse rate, so the chapter's own QEMU
 * verification run can show several real, independent tick prints in
 * a few seconds of wall-clock time without flooding the serial log. */
#define TICKS_PER_PRINT 100

void pit_init(uint32_t frequency_hz) {
    uint32_t divisor = PIT_BASE_FREQUENCY_HZ / frequency_hz;

    outb(PIT_COMMAND_PORT, PIT_CH0_MODE3_BINARY);
    /* "Lobyte/hibyte" access mode (the 11 in bits 5-4 above) means the
     * PIT expects the 16-bit divisor as two separate byte writes to the
     * same data port, low byte first. */
    outb(PIT_CH0_DATA_PORT, (uint8_t) (divisor & 0xFF));
    outb(PIT_CH0_DATA_PORT, (uint8_t) ((divisor >> 8) & 0xFF));

    /* Unmask IRQ0 only now that channel 0 is actually programmed and
     * ticking at a known rate -- the same reasoning 005_keyboard.c's
     * keyboard_init() already applied to IRQ1: never unmask a line
     * before the hardware behind it is in a state this kernel actually
     * understands. */
    pic_clear_mask(0);
}

uint32_t pit_get_ticks(void) {
    return tick_count;
}

/* Called from 024_irq0.asm's own stub on every real IRQ0. Chapter 11
 * ended here -- this chapter adds exactly one more call, task_tick(),
 * and its position relative to pic_send_eoi(0) is not arbitrary. EOI
 * has to be sent *before* task_tick() can possibly switch this CPU
 * away to another task, not after: if a switch happens first and this
 * exact call never returns for a long while (this task might not run
 * again for many more real ticks), the master PIC would never be told
 * this IRQ0 was handled, and "the PIC will never deliver that line
 * ... again" (011_pic.h's own words for exactly this requirement) --
 * meaning every *other* task would also stop receiving ticks, since
 * the one thing that could eventually get this task rescheduled to
 * finish sending that EOI is itself a tick. Sending it first, always,
 * before anything that might switch away, avoids that outright. */
void irq0_handler(void) {
    tick_count++;

    if (tick_count % TICKS_PER_PRINT == 0) {
        kprintf("tick: %u\n", tick_count);
    }

    pic_send_eoi(0);
    task_tick();
}
