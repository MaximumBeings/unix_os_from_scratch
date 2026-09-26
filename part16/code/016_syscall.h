#ifndef UNIX_OS_016_SYSCALL_H
#define UNIX_OS_016_SYSCALL_H

/* This chapter's whole syscall table: three real numbers now, shared
 * between every ring-3 caller (016_kmain.c's own ring3_task_a_entry()/
 * ring3_task_b_entry(), via the shared usermode_syscall() helper, which
 * loads one of these into EAX before executing INT 0x80) and the
 * ring-0 dispatcher (016_isr_handlers.c's isr128_handler(), which
 * switches on it). OSDev Wiki, "System Calls": "If all function codes
 * are small contiguous numbers, a better option might be a function
 * table" -- still overkill for exactly three real syscalls, but the
 * constants still live here rather than as bare magic numbers
 * scattered across two different files.
 *
 * SYS_YIELD and SYS_EXIT are new this chapter, and neither one does
 * any real work of its own: both are thin ring-0 wrappers around
 * functions this book has had since Chapter 11/12 -- task_yield() and
 * task_exit() -- reused exactly as-is. Nothing about the scheduler
 * itself needed to change to let ring-3 code drive it; only a door
 * back into ring 0 was missing, and INT 0x80 (016_isr128.asm) was
 * already that door for SYS_WRITE_STR. */
#define SYS_WRITE_STR 1u
#define SYS_YIELD     2u
#define SYS_EXIT      3u

#endif
