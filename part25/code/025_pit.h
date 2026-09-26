#ifndef UNIX_OS_025_PIT_H
#define UNIX_OS_025_PIT_H

#include <stdint.h>

/* Programs PIT channel 0 for Mode 3 (square wave generator) at
 * approximately frequency_hz interrupts per second, then unmasks IRQ0
 * on the PIC (025_pic.c) so those interrupts can actually reach this
 * kernel. Call only after pic_remap() and pic_disable_all() have
 * already run, and before this kernel enables interrupts globally with
 * STI -- the same ordering 025_keyboard.h's keyboard_init() already
 * requires for IRQ1. */
void pit_init(uint32_t frequency_hz);

/* The number of real IRQ0 ticks delivered since pit_init() ran. */
uint32_t pit_get_ticks(void);

/* The real C handler 025_irq0.asm's stub calls on every IRQ0. Since
 * this chapter, also drives this kernel's own preemptive scheduling
 * -- see 025_pit.c's own comment on why task_tick() is called only
 * after this tick's EOI has already been sent. */
void irq0_handler(void);

#endif
