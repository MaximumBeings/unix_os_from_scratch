/* Chapter 5: this book's first real exception handler (isr0_handler,
 * unchanged since). Chapter 10 adds this book's second: isr14_handler,
 * for #PF -- and unlike #DE, a page fault carries real, decodable
 * information about exactly what went wrong, which this handler reads
 * and prints rather than just reporting "a fault happened."
 *
 * Like isr0_handler, this one does not try to resume the faulting
 * instruction -- this book has no demand paging, no swapping, nothing
 * that could make an unmapped or protected address valid by the time
 * this handler returns, so retrying would just fault again on the
 * same instruction, forever. Reporting the fault honestly and halting
 * is still the right call until a later chapter gives this kernel a
 * real mechanism to fix the underlying mapping and actually resume. */

#include "012_isr_handlers.h"
#include "012_printf.h"

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

/* Error code bit layout, cited field-for-field:
 *   P (bit 0): "When set, the page fault was caused by a
 *               page-protection violation. When not set, it was
 *               caused by a non-present page."
 *   W (bit 1): "When set, the page fault was caused by a write
 *               access. When not set, it was caused by a read access."
 *   U (bit 2): "When set, the page fault was caused while CPL = 3."
 * (OSDev Wiki, "Exceptions", vector 14 / #PF: https://wiki.osdev.org/Exceptions)
 *
 * The faulting linear address itself is not part of the error code at
 * all -- the same page notes "it sets the value of the CR2 register
 * to the virtual address which caused the Page Fault," so this
 * handler reads CR2 directly rather than looking for it on the stack. */
void isr14_handler(uint32_t error_code) {
    uint32_t faulting_address;
    __asm__ volatile ("mov %%cr2, %0" : "=r" (faulting_address));

    kprintf("\n*** CPU EXCEPTION: Page Fault (vector 14, #PF) ***\n");
    kprintf("Faulting address (CR2): 0x%x\n", faulting_address);
    kprintf("Error code: 0x%x (%s, %s, %s)\n",
            error_code,
            (error_code & 0x1u) ? "protection violation" : "non-present page",
            (error_code & 0x2u) ? "write" : "read",
            (error_code & 0x4u) ? "user mode" : "supervisor mode");
    kprintf("This handler does not resume the faulting instruction --\n");
    kprintf("halting.\n");

    __asm__ volatile ("cli");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
