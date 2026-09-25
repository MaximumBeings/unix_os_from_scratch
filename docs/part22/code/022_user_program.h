#ifndef UNIX_OS_022_USER_PROGRAM_H
#define UNIX_OS_022_USER_PROGRAM_H

/* The one number 022_user_program.c and 022_kmain.c both need to agree
 * on, and the reason this tiny header exists at all: how many times
 * the real, separately compiled user program below prints and yields
 * before its own real SYS_EXIT. Every earlier chapter's own per-process
 * iteration count (Chapter 16's, Chapter 17's own PROCESS_TASK_
 * ITERATIONS) could simply live as a #define inside 022_kmain.c itself,
 * because the code that looped on it and the code that computed an
 * independently-checkable expected switch count from it were the SAME
 * translation unit, built by the SAME compiler invocation. This
 * chapter's own user program is not: it is compiled and linked
 * completely separately from this kernel image (022_user_program.c's
 * own dedicated linker script, 022_user_program.ld, never even mentions
 * 022_kmain.c), so the one thing keeping this book's own real switch-
 * count arithmetic honest is a header both real, independent builds
 * `#include` -- exactly the role a real kernel's own uapi header plays
 * between kernel and userspace source trees that are compiled apart
 * from one another but still have to agree on shared constants. */
#define USER_PROGRAM_ITERATIONS 5u

#endif
