#ifndef UNIX_OS_035_SPINLOCK_H
#define UNIX_OS_035_SPINLOCK_H

#include <stdint.h>

/* Chapter 12 made this kernel preemptible: a real IRQ0 tick can now
 * suspend whichever task is running, at any instruction boundary, and
 * hand the CPU to another task. That is exactly what Chapter 9's own
 * kmalloc()/kfree() were never written to survive -- see 035_kheap.c
 * for the real corruption this chapter captures before fixing it.
 * This is this kernel's first synchronization primitive: something a
 * critical section can hold to guarantee it runs to completion before
 * any other task's code can touch the same data. */
typedef struct {
    int locked;
} spinlock_t;

void spinlock_init(spinlock_t *lock);

/* Acquires `lock` and returns the caller's EFLAGS exactly as they
 * were the instant before this call. Pass that value back to
 * spinlock_release() for this same critical section, unchanged --
 * see 035_spinlock.c for why this kernel saves and restores EFLAGS
 * itself rather than blindly re-enabling interrupts on release. */
uint32_t spinlock_acquire(spinlock_t *lock);

/* Releases `lock`, restoring EFLAGS (and therefore IF) to exactly the
 * value the matching spinlock_acquire() call returned. */
void spinlock_release(spinlock_t *lock, uint32_t saved_eflags);

#endif
