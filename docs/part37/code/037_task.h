#ifndef UNIX_OS_037_TASK_H
#define UNIX_OS_037_TASK_H

#include <stdint.h>

/* The one fixed virtual address every process's own code lives at,
 * and the one fixed virtual address every process's own stack lives
 * at -- the SAME two numbers for every process this kernel ever
 * creates, each one backed by a different, private physical frame per
 * process (037_task.c's own task_create_process()). Chosen well
 * outside every earlier chapter's own special virtual addresses
 * (0xC0000000 -- Chapter 8's own demo mapping; the old, since-removed
 * Chapter 10 fault-trigger address 0xE0000000) so nothing about this
 * chapter's own real captured evidence could be mistaken for reusing
 * either. PROCESS_STACK_VADDR's own top -- what actually gets handed
 * to enter_usermode() -- is PROCESS_STACK_VADDR + 4096. */
#define PROCESS_CODE_VADDR  0xE8000000u
#define PROCESS_STACK_VADDR 0xE8001000u

/* Sets up this kernel's task list with exactly one task -- task 0, the
 * kernel's own boot-time execution context (the one kmain() is already
 * running on when this is called), still running on the same boot
 * stack every chapter since Chapter 1 has used. Must be called before
 * any task_create()/task_yield()/task_exit() call. */
void task_init(void);

/* Creates a new cooperative kernel task: allocates it a real stack
 * from Chapter 9's kmalloc(), and pre-populates that stack so the
 * first time this task is ever switched into, execution begins at
 * `entry` (OSDev Wiki, "X86 Cooperative Multitasking Tutorial": "the
 * kernel needs to put values on the new task's kernel stack to match
 * the values that 'switch_tasks' expects to pop off"). `entry` must
 * eventually call task_exit() -- it must never return normally.
 * Returns the new task's index (1, 2, ...), or -1 if this kernel's
 * fixed task table is already full. */
int task_create(void (*entry)(void));

/* Creates a new RING-3 task with its own REAL, PRIVATE address space
 * -- new this chapter, and the reason "usermode" from Chapter 15-16
 * has become "process" here. Every earlier ring-3 task, including
 * Chapter 16's own two, shared this kernel's ONE page directory --
 * genuinely running at CPL 3, but still able, in principle, to reach
 * any other ring-3 task's own memory just by knowing its address,
 * since every ring-3 mapping lived in the same one shared directory.
 * task_create_process() instead calls paging_new_address_space() to
 * give this task a page directory nothing else in this kernel shares,
 * physically COPIES `template_size` bytes starting at `template_start`
 * -- a whole, page-aligned, self-contained function, one that makes no
 * calls to anything outside itself, since a copied function's own
 * internal jumps/branches stay correct at a new address (x86 near
 * jumps are PC-relative) but a CALL to code that did NOT get copied
 * alongside would carry the wrong PC-relative displacement -- into a
 * brand-new, private physical frame, and maps that COPY into this
 * task's own new directory at the fixed virtual address
 * PROCESS_CODE_VADDR -- the same fixed address every process built
 * this way uses for its own code, each one backed by its own real,
 * separate physical frame. Still real infrastructure this chapter
 * carries forward unchanged, even though 037_kmain.c's own demo this
 * chapter uses the new task_create_elf_process() below instead. A
 * second private frame,
 * mapped at the fixed virtual address PROCESS_STACK_VADDR, is this
 * task's own real user stack. Builds the same dedicated kernel stack
 * every ring-3 task has needed since Chapter 16 (switch_task()'s own
 * stack, and this task's own TSS.ESP0 target). Returns the new task's
 * index, or -1 if the task table is already full. */
int task_create_process(const void *template_start, uint32_t template_size);

/* The physical address of the page directory task_create_process()
 * built for the process at `index` -- new this chapter, purely so
 * 037_kmain.c's own kmain() can call paging_translate_in() itself
 * and prove, from ring 0, that PROCESS_CODE_VADDR resolves to a
 * different real physical frame in two different processes' own
 * directories, before either process ever actually runs. Returns 0
 * for any task that is not a process (task_create()'s own tasks, and
 * task 0) -- the same "0 means the kernel's own default directory"
 * convention struct task itself uses internally. */
uint32_t task_page_directory_phys(int index);

/* New this chapter: creates a process from a REAL, separately
 * compiled ELF executable -- `module_start`/`module_end`, the real
 * physical address range GRUB already loaded the module's raw bytes
 * into (037_multiboot.h's own multiboot_tag_module), before this
 * kernel ever ran a single instruction. Builds this task's own kernel
 * stack and private address space exactly like task_create_process()
 * above, but instead of copying one fixed, kernel-image-resident
 * template function to one fixed virtual address, calls 037_elf.c's
 * own elf_load() to walk the module's real ELF program headers and
 * map every one of its real PT_LOAD segments -- however many there
 * are, at whatever virtual addresses the FILE ITSELF specifies, not a
 * constant this kernel chooses. This task's own entry point is
 * therefore not PROCESS_CODE_VADDR at all, but the file's own real
 * e_entry, read out of the ELF header at load time. Still uses the
 * fixed PROCESS_STACK_VADDR convention for this task's own user
 * stack, exactly like task_create_process() -- the stack is this
 * kernel's own concern, never the loaded file's. Returns the new
 * task's index, or -1 if the task table is already full or the module
 * fails to parse as a valid ELF32 executable (037_elf.c's own
 * elf_load() reports the specific reason). */
int task_create_elf_process(uint32_t module_start, uint32_t module_end);

/* Voluntarily gives up the CPU: saves the calling task's callee-saved
 * registers and stack pointer, picks the next RUNNING task in
 * round-robin order, and switches to it. Returns normally, right
 * where it left off, once this task is switched back in. If no other
 * task is currently RUNNING, returns immediately without performing a
 * real switch. */
void task_yield(void);

/* Marks the calling task DONE and switches away from it permanently.
 * Never returns -- this task's own stack and call frame are simply
 * abandoned from this point on. */
void task_exit(void);

/* Non-zero once the task at `index` (as returned by task_create())
 * has called task_exit(). */
int task_is_done(int index);

/* The currently-running task's own index -- what task_create() itself
 * returned when this task was created (0 for the kernel's own boot
 * task). New this chapter, so 037_semaphore.c can record which task
 * is waiting on which semaphore without task.c having to know
 * anything about semaphores itself. */
int task_current_id(void);

/* Marks the CALLING task BLOCKED and returns immediately -- it does
 * NOT yield the CPU itself. Split out from a single "block and yield"
 * call on purpose: 037_semaphore.c needs this exact state transition
 * to happen while it still holds its own lock, so that no concurrent
 * semaphore_signal() can dequeue this task as a waiter before the
 * scheduler has actually stopped considering it runnable (see
 * 037_semaphore.c's own comments for the real race this avoids). The
 * caller is expected to give up the CPU with task_yield() itself,
 * separately, once it is safe to do so. A BLOCKED task is skipped by
 * every future task_yield()/task_tick() scan until some other task
 * calls task_wake() on it. */
void task_block_self(void);

/* Marks the task at `index` READY again, making it eligible to be
 * picked by task_yield()'s own round-robin scan the next time it is
 * that task's turn -- it does NOT itself trigger an immediate switch
 * to that task. Called from 037_semaphore.c's semaphore_signal(),
 * always from inside that semaphore's own lock (interrupts already
 * off), so this needs no locking of its own. */
void task_wake(int index);

/* Total number of real context switches switch_task() has performed
 * since task_init(). Exists purely for this chapter's own real, live
 * verification. */
uint32_t task_switch_count(void);

/* Called from 037_pit.c's irq0_handler() on every single real IRQ0
 * tick, after that tick's own EOI has already been sent. Before
 * task_init() has run, does nothing -- this kernel has no task table
 * to preempt yet. After task_init(), calls task_yield() unconditionally,
 * exactly once per real tick: this chapter's whole scheduling policy
 * is "one tick, one time slice," chosen because it is the simplest
 * policy whose real switch count is independently predictable from
 * nothing but the real tick count, with no separate countdown state
 * of its own to get wrong. */
void task_tick(void);

#endif
