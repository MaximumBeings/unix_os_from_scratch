#ifndef UNIX_OS_028_TSS_H
#define UNIX_OS_028_TSS_H

#include <stdint.h>

/* This chapter's Task State Segment -- not used the way its name
 * suggests. The CPU's own hardware task-switching mechanism (a full
 * TSS per task, switched via a far JMP/CALL) is not what this is
 * for; this book still does software task switching entirely by hand
 * (028_switch.asm, unchanged since Chapter 11). This ONE TSS exists
 * purely so the CPU has somewhere to find a real kernel stack the
 * instant a ring-3 task raises an interrupt or exception -- see
 * 028_tss.c for the real mechanics. */

/* Zeroes this chapter's one static TSS and sets its SS0 field to this
 * kernel's own kernel-data selector -- the segment every privilege-
 * elevating interrupt switches to. Must be called before gdt_init(),
 * which installs this same TSS's address into the GDT. */
void tss_init(void);

/* Updates ESP0: the exact stack pointer the CPU loads, from this
 * TSS, the instant a ring-3 task raises ANY interrupt or exception --
 * a syscall, a timer tick, a fault, all of it. Must be set to a real,
 * currently-unused kernel stack before ever entering ring 3; calling
 * it again before entering ring 3 a second time lets a later chapter
 * give each task its own dedicated kernel stack. */
void tss_set_kernel_stack(uint32_t esp0);

/* Loads the Task Register with `selector` via LTR -- the one real
 * machine instruction that tells the CPU which GDT entry is its own
 * active TSS. Must be called only after gdt_init() has already
 * installed a valid TSS descriptor at that exact selector. */
void tss_load(uint16_t selector);

/* This TSS's own address and real size, in bytes -- read by
 * 028_gdt.c to build its GDT descriptor, so the TSS's own struct
 * layout stays private to 028_tss.c. */
const void *tss_get_address(void);
uint32_t tss_get_size(void);

#endif
