#ifndef UNIX_OS_037_SYSCALL_H
#define UNIX_OS_037_SYSCALL_H

/* This chapter's whole syscall table: three real numbers, unchanged
 * since Chapter 16, shared between every ring-3 caller (037_kmain.c's
 * own process_template_normal()/process_template_reckless(), each
 * one loading a number straight into EAX before its own inlined INT
 * 0x80 -- no shared helper function this chapter, see those
 * functions' own comments on why) and the ring-0 dispatcher
 * (037_isr_handlers.c's isr128_handler(), which switches on it).
 * OSDev Wiki, "System Calls": "If all function codes are small
 * contiguous numbers, a better option might be a function table" --
 * still overkill for exactly three real syscalls, but the constants
 * still live here rather than as bare magic numbers scattered across
 * two different files.
 *
 * SYS_YIELD and SYS_EXIT are thin ring-0 wrappers around functions
 * this book has had since Chapter 11/12 -- task_yield() and
 * task_exit() -- reused exactly as-is. Nothing about the scheduler
 * itself needed to change to let ring-3 code drive it; only a door
 * back into ring 0 was missing, and INT 0x80 (037_isr128.asm) was
 * already that door for SYS_WRITE_STR. */
#define SYS_WRITE_STR 1u
#define SYS_YIELD     2u
#define SYS_EXIT      3u

#endif
