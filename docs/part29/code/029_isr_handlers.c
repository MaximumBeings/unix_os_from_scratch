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

#include "029_isr_handlers.h"
#include "029_printf.h"
#include "029_syscall.h"
#include "029_task.h"

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

/* This chapter's third real exception handler, and this book's first
 * one triggered from ring 3 rather than ring 0. Error code bit layout,
 * cited field-for-field (OSDev Wiki, "Exceptions", vector 13 / #GP):
 *   "The General Protection Fault sets an error code, which is the
 *   segment selector index when the exception is segment related.
 *   Otherwise, 0." -- when non-zero, bit 0 (E) means "the exception
 *   originated externally to the processor," bits 2-1 (Tbl) name
 *   which table the index refers to (GDT/IDT/LDT), and bits 31-3
 *   (Index) are that table's own real index.
 *
 * 029_kmain.c's own ring-3 demo triggers this by executing CLI at
 * CPL 3 -- one of the "Executing a privileged instruction while
 * CPL != 0" triggers the same page lists -- which is NOT segment-
 * related, so this handler's own real captured run shows an error
 * code of exactly 0, not a decoded selector. The decoding logic below
 * still exists for the general case, the same honest-by-default
 * style every other error-code handler in this book already follows. */
void isr13_handler(uint32_t error_code) {
    kprintf("\n*** CPU EXCEPTION: General Protection Fault (vector 13, #GP) ***\n");
    if (error_code == 0) {
        kprintf("Error code: 0x0 (not segment-related -- e.g. a privileged "
                "instruction executed at CPL != 0)\n");
    } else {
        uint32_t table = (error_code >> 1) & 0x3u;
        kprintf("Error code: 0x%x (external=%u, table=%s, index=%u)\n",
                error_code,
                error_code & 0x1u,
                (table == 0) ? "GDT" : (table == 1) ? "IDT" : "LDT",
                (error_code >> 3) & 0x1FFFu);
    }
    kprintf("This handler does not resume the faulting instruction --\n");
    kprintf("halting.\n");

    __asm__ volatile ("cli");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

/* This chapter's real syscall dispatcher -- the whole reason ring 3
 * can do anything useful at all despite every port I/O instruction
 * and every privileged instruction being forbidden to it. Runs at
 * ring 0, called from 029_isr128.asm's own INT 0x80 stub with
 * whatever the caller put in EAX/EBX forwarded as real cdecl
 * arguments. Now a real switch statement, exactly as 029_syscall.h's
 * own comment (and OSDev Wiki, "System Calls": "If all function codes
 * are small contiguous numbers, a better option might be a function
 * table") anticipated for whenever a second syscall showed up --
 * this chapter adds two at once.
 *
 * SYS_YIELD and SYS_EXIT are both genuinely nothing more than a direct
 * call into 029_task.c's own existing task_yield()/task_exit() --
 * unchanged since Chapter 11/12, and unaware that this particular call
 * came from a ring-3 syscall rather than an ordinary ring-0 call site.
 * That is the entire point of building this chapter's ring-3 support
 * on top of Chapter 11's scheduler rather than beside it: a ring-3
 * task's SYS_YIELD is a REAL task_yield() call, picking the next READY
 * task in exactly the same round-robin scan every earlier chapter's
 * own ring-0 tasks already go through, and its SYS_EXIT is a REAL
 * task_exit() call, marking this exact task DONE in the very same
 * fixed task table.
 *
 * task_exit() never returns (see 029_task.c's own comment on it), so
 * the SYS_EXIT case below never falls through to 029_isr128.asm's own
 * `add esp, 8 / popa / iret` epilogue -- this task's entire kernel
 * stack, including this exact call frame, is simply abandoned at that
 * point, exactly like every ring-0 task's own task_exit() call already
 * does. */
void isr128_handler(uint32_t syscall_num, uint32_t arg) {
    switch (syscall_num) {
    case SYS_WRITE_STR:
        kprintf("%s", (const char *) (uintptr_t) arg);
        break;
    case SYS_YIELD:
        task_yield();
        break;
    case SYS_EXIT:
        task_exit();
        break;  /* unreachable: task_exit() never returns */
    default:
        kprintf("\n*** isr128_handler: unknown syscall number %u -- ignoring ***\n",
                syscall_num);
        break;
    }
}
