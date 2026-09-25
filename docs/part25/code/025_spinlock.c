#include <stdint.h>

#include "025_printf.h"
#include "025_spinlock.h"

/* The OSDev Wiki's own spinlock is built for real multiprocessor
 * mutual exclusion: "a spinlock is a type of reentrancy lock, where
 * the CPU repeatedly attempts to acquire the lock until it succeeds",
 * implemented with an atomic read-modify-write such as
 * "lock bts [lock],0" / "jc .retry" to acquire and "lock btr [lock],0"
 * to release (OSDev Wiki, "Spinlock":
 * https://wiki.osdev.org/Spinlock) -- because on real multiprocessor
 * hardware, another CPU can be touching the exact same memory at the
 * exact same instant, and only an atomic instruction (BTS/BTR, XCHG,
 * CMPXCHG) can test and set that flag as one indivisible step. The
 * OSDev Wiki's "Synchronization Primitives" page describes the same
 * requirement in C11 terms: "a spinlock will keep checking the value
 * until it has changed and usually relies on some atomic test_and_set
 * instruction" (https://wiki.osdev.org/Synchronization_Primitives).
 *
 * This kernel is not multiprocessor. Every chapter of this book so
 * far has run on exactly one CPU, and nothing in this codebase starts
 * a second one. On a single CPU, the only thing that can ever
 * interrupt a critical section mid-way and run different code is a
 * hardware interrupt -- and Chapter 12 already established that the
 * *only* source of task preemption in this kernel is task_tick(),
 * called exclusively from 025_pit.c's irq0_handler(), which cannot
 * fire while IF=0. So on this single CPU, disabling interrupts for
 * the duration of a critical section is already sufficient to
 * guarantee no other task's code can run until this one re-enables
 * them -- no atomic test-and-set instruction is needed to make that
 * true, because nothing else is executing concurrently for one to
 * race against. This is the same reasoning real single-CPU kernels
 * have long relied on for their cheapest locks; Linux's own
 * spin_lock_irqsave()/spin_unlock_irqrestore() pair -- save the
 * caller's EFLAGS, disable interrupts, do the work, then restore
 * exactly the EFLAGS that were saved -- is the general shape this
 * chapter's own spinlock_acquire()/spinlock_release() follow, though
 * that specific pairing is this book's own engineering reasoning
 * about this kernel's own single-CPU threat model, not a wiki quote.
 *
 * Saving and restoring the caller's actual EFLAGS, rather than always
 * doing an unconditional `sti` on release, matters for exactly the
 * same reason 025_task.c's task_yield() does not just `sti`
 * unconditionally either: a spinlock_acquire() reached from code that
 * already had interrupts disabled for its own reasons (nested inside
 * some other critical section, or inside an ISR) must not force
 * interrupts back on underneath that caller when this lock releases
 * -- it must restore IF to whatever it actually was before this call. */

void spinlock_init(spinlock_t *lock) {
    lock->locked = 0;
}

uint32_t spinlock_acquire(spinlock_t *lock) {
    uint32_t saved_eflags;
    __asm__ volatile ("pushf\n\t"
                       "pop %0"
                       : "=r"(saved_eflags));
    __asm__ volatile ("cli");

    if (lock->locked) {
        /* On this single CPU, with interrupts already disabled from
         * this exact point on, nothing else this kernel runs could
         * possibly be holding this lock right now: task_yield() is
         * only ever reached from task_tick(), and task_tick() is only
         * ever reached from inside irq0_handler(), which cannot fire
         * while IF=0. Finding `locked` already set here is therefore
         * not real contention from another task -- it means some
         * earlier spinlock_acquire() on this exact lock was never
         * matched by a spinlock_release(), which is a real bug in
         * this kernel's own code, not a race to wait out. There is
         * nothing correct to do but say so and stop. */
        kprintf("spinlock_acquire: lock already held -- caller bug, halting\n");
        for (;;) {
            __asm__ volatile ("hlt");
        }
    }

    lock->locked = 1;
    return saved_eflags;
}

void spinlock_release(spinlock_t *lock, uint32_t saved_eflags) {
    lock->locked = 0;
    __asm__ volatile ("push %0\n\t"
                       "popf"
                       :
                       : "r"(saved_eflags)
                       : "memory", "cc");
}
