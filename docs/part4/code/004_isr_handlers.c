/* Chapter 4: this book's first real exception handler. It does not try
 * to resume the instruction that faulted -- for a divide-by-zero, there
 * is nothing to fix and no sane value to hand back to the IDIV
 * instruction that trapped, so returning via IRET would just fault
 * again on the same instruction, forever. Reporting the fault and
 * halting is the honest thing to do until a later chapter gives this
 * kernel a real reason and a real mechanism to recover instead. */

#include "004_isr_handlers.h"
#include "004_printf.h"

void isr0_handler(void) {
    kprintf("\n*** CPU EXCEPTION: Divide Error (vector 0, #DE) ***\n");
    kprintf("A DIV or IDIV instruction attempted to divide by zero.\n");
    kprintf("This handler does not resume the faulting instruction --\n");
    kprintf("halting.\n");

    __asm__ volatile ("cli");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
