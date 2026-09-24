#ifndef UNIX_OS_015_ISR_HANDLERS_H
#define UNIX_OS_015_ISR_HANDLERS_H

#include <stdint.h>

/* The real C handler 015_isr0.asm's stub calls on a #DE fault. */
void isr0_handler(void);

/* The real C handler 015_isr13.asm's stub calls on a #GP fault, with
 * the CPU's own real error code passed as its one argument. This
 * chapter's own real proof that a ring-3 task is genuinely restricted:
 * see 015_kmain.c's user_mode_entry() for what deliberately triggers
 * it. */
void isr13_handler(uint32_t error_code);

/* The real C handler 015_isr14.asm's stub calls on a #PF fault, with
 * the CPU's own real error code passed as its one argument. */
void isr14_handler(uint32_t error_code);

/* The real C handler 015_isr128.asm's stub calls on this chapter's
 * own INT 0x80 syscall gate: `syscall_num` is whatever the caller put
 * in EAX (015_syscall.h's own SYS_WRITE_STR, so far the only one that
 * exists), `arg` is whatever it put in EBX -- for SYS_WRITE_STR, a
 * pointer to a NUL-terminated string. This handler runs at ring 0
 * (the whole reason the transition just happened), so dereferencing
 * `arg` here is always safe regardless of whether that memory is
 * itself marked user-accessible: ring 0 can read any PRESENT page,
 * user-accessible or not -- only ring 3 is ever restricted by the U/S
 * bit. */
void isr128_handler(uint32_t syscall_num, uint32_t arg);

#endif
