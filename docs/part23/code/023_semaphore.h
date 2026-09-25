#ifndef UNIX_OS_023_SEMAPHORE_H
#define UNIX_OS_023_SEMAPHORE_H

#include <stdint.h>

#include "023_spinlock.h"

/* This chapter's task table has room for at most 9 tasks
 * (023_task.c's own TASK_MAX_TASKS) -- so at most 9 tasks could ever
 * be waiting on any one semaphore at the same time. A fixed-size
 * array sized to that same real bound is enough; nothing here ever
 * needs to grow it. */
#define SEMAPHORE_MAX_WAITERS 9u

/* A real counting semaphore: an opaque integer with exactly two
 * operations, wait and signal (OSDev Wiki, "Semaphore":
 * "it is an opaque data type with two defined operations, usually
 * called wait and signal" -- https://wiki.osdev.org/Semaphore). Its
 * own `count` and waiter queue are protected by Chapter 13's own
 * spinlock, not by anything new -- a semaphore is built ON TOP of a
 * spinlock in this kernel, not an alternative to one. */
typedef struct {
    int count;
    spinlock_t lock;
    int waiters[SEMAPHORE_MAX_WAITERS];
    int waiter_head;
    int waiter_len;
} semaphore_t;

/* Initializes `sem` with `initial_count` units immediately available
 * (0 is a normal, common starting count -- it means every call to
 * semaphore_wait() blocks until something first calls
 * semaphore_signal()). */
void semaphore_init(semaphore_t *sem, int initial_count);

/* Takes one unit from `sem`. If one is available right now, returns
 * immediately having taken it. If not, genuinely blocks the calling
 * task -- taking it off the CPU entirely via task_block_self() and
 * task_yield(), not spinning -- until some other task's
 * semaphore_signal() call on this exact semaphore hands one to it
 * directly. */
void semaphore_wait(semaphore_t *sem);

/* Returns one unit to `sem`. If a task is already waiting, wakes the
 * one that has been waiting longest and hands the unit directly to
 * it, without ever incrementing `count` at all (OSDev Wiki,
 * "Semaphore": "the thread at the front of the queue is woken and
 * rescheduled"). Only increments `count` when no task is waiting. */
void semaphore_signal(semaphore_t *sem);

#endif
