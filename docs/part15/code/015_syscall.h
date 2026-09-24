#ifndef UNIX_OS_015_SYSCALL_H
#define UNIX_OS_015_SYSCALL_H

/* This chapter's entire syscall table: one real number, shared
 * between the ring-3 caller (015_kmain.c's own user_mode_entry(),
 * which loads it into EAX before executing INT 0x80) and the ring-0
 * dispatcher (015_isr_handlers.c's isr128_handler(), which switches
 * on it). OSDev Wiki, "System Calls": "If all function codes are
 * small contiguous numbers, a better option might be a function
 * table" -- overkill for exactly one real syscall, but the constant
 * still lives here rather than as a bare magic number in two
 * different files, ready for that table the moment a second syscall
 * shows up. */
#define SYS_WRITE_STR 1u

#endif
