#include <stdint.h>

#include "021_printf.h"
#include "021_semaphore.h"
#include "021_task.h"

/* A waiting task's own index is recorded in a plain FIFO ring buffer
 * -- OSDev Wiki, "Semaphore": "the thread at the front of the queue is
 * woken and rescheduled", so this only ever needs to support "add to
 * the back" and "remove from the front", never a search or a
 * removal from the middle. */
static void enqueue_waiter(semaphore_t *sem, int task_id) {
    int tail = (sem->waiter_head + sem->waiter_len) % SEMAPHORE_MAX_WAITERS;
    sem->waiters[tail] = task_id;
    sem->waiter_len++;
}

static int dequeue_waiter(semaphore_t *sem) {
    int task_id = sem->waiters[sem->waiter_head];
    sem->waiter_head = (sem->waiter_head + 1) % SEMAPHORE_MAX_WAITERS;
    sem->waiter_len--;
    return task_id;
}

void semaphore_init(semaphore_t *sem, int initial_count) {
    sem->count = initial_count;
    spinlock_init(&sem->lock);
    sem->waiter_head = 0;
    sem->waiter_len = 0;
}

void semaphore_wait(semaphore_t *sem) {
    uint32_t saved_eflags = spinlock_acquire(&sem->lock);

    if (sem->count > 0) {
        sem->count--;
        spinlock_release(&sem->lock, saved_eflags);
        return;
    }

    /* Nothing available right now. OSDev Wiki, "Semaphore": "A thread
     * calling wait on a semaphore whose value is 0 should be removed
     * from the scheduler queue, and only added to it again when the
     * semaphore is signalled." (https://wiki.osdev.org/Semaphore)
     *
     * The naive way to do that is to record this task as a waiter,
     * unlock, and THEN call some "block myself and yield" helper.
     * That has a real bug -- the classic lost-wakeup race: the instant
     * this function releases `sem->lock`, interrupts come back on, and
     * a real IRQ0 tick could preempt this exact task into some other
     * one that happens to call semaphore_signal() on this same `sem`
     * before this task has actually marked itself BLOCKED. That
     * signal() would dequeue this task and call task_wake() on it --
     * which is a harmless no-op if this task is still READY at that
     * point -- and hand off its one unit believing this task received
     * it. If THEN, only after all that, this task finally marked
     * itself BLOCKED and yielded, it would go to sleep having already
     * been given its unit, with nothing left to ever wake it again.
     *
     * The fix is to make "become a waiter" and "become unschedulable"
     * happen as one atomic step, both still inside this function's own
     * lock: task_block_self() only sets this task's own state, it does
     * not yield the CPU, so by the time `sem->lock` is released below,
     * any concurrent semaphore_signal() will see this task already
     * BLOCKED and correctly wake it back up rather than racing past
     * it. Only after unlocking does this task actually give up the
     * CPU, via its own ordinary task_yield() call -- and if a tick
     * preempts it in the gap between those two lines, task_yield()
     * itself will simply find this task already BLOCKED and skip it,
     * which is exactly correct. */
    int self = task_current_id();
    enqueue_waiter(sem, self);
    task_block_self();
    /* Printed while `sem->lock` is still held -- interrupts are still
     * off here, on this single CPU, so nothing else can run between
     * this task marking itself BLOCKED and this line actually
     * executing. That guarantees this message always appears before
     * any semaphore_signal() call could possibly wake this exact task
     * and print its own "waking" message; printing after unlocking
     * would leave a real (if rare) chance of the two appearing in the
     * confusing order. */
    kprintf("  semaphore_wait: task %d blocking (no units available)\n", self);

    spinlock_release(&sem->lock, saved_eflags);

    task_yield();
    /* Resumes here once some later semaphore_signal() has woken this
     * exact task and the round-robin scan has reached it again -- with
     * its one unit already accounted for by that signal() call, not by
     * anything this function does after waking up. */
}

void semaphore_signal(semaphore_t *sem) {
    uint32_t saved_eflags = spinlock_acquire(&sem->lock);

    if (sem->waiter_len > 0) {
        int woken = dequeue_waiter(sem);
        task_wake(woken);
        spinlock_release(&sem->lock, saved_eflags);
        kprintf("  semaphore_signal: waking task %d\n", woken);
        return;
    }

    sem->count++;
    spinlock_release(&sem->lock, saved_eflags);
}
