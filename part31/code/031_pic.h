#ifndef UNIX_OS_031_PIC_H
#define UNIX_OS_031_PIC_H

#include <stdint.h>

/* Moves the two 8259 PICs' interrupt vectors off their legacy, BIOS-era
 * default (which collides with the CPU's own reserved exception
 * vectors) and onto offset1/offset2 instead -- see 031_pic.c for why
 * this has to happen before any hardware interrupt can be enabled at
 * all. */
void pic_remap(int offset1, int offset2);

/* Masks every one of the 15 usable IRQ lines. Called right after
 * pic_remap(), before any line this kernel actually has a handler for
 * is deliberately unmasked -- see 031_pic.c for why the remap step
 * alone does not leave the mask register in a safe, known state. */
void pic_disable_all(void);

/* Masks (disables) or unmasks (enables) one IRQ line, 0-15, on
 * whichever PIC actually owns it. */
void pic_set_mask(uint8_t irq_line);
void pic_clear_mask(uint8_t irq_line);

/* Tells the PIC that this kernel is done handling the interrupt it
 * just delivered -- required after every IRQ, or the PIC will never
 * deliver that line (or, for IRQ lines 0-7, any line at or below it)
 * again. */
void pic_send_eoi(uint8_t irq_line);

#endif
