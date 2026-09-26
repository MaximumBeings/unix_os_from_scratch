#include <stdint.h>

#include "032_elf.h"
#include "032_kheap.h"
#include "032_paging.h"
#include "032_pmm.h"
#include "032_printf.h"
#include "032_task.h"
#include "032_tss.h"

/* This chapter's own Thread Control Block, in the same spirit the
 * OSDev Wiki describes for kernel-level tasks: "The TCB holds task
 * state including the kernel stack pointer (ESP) ... " (OSDev Wiki,
 * "Kernel Multitasking": https://wiki.osdev.org/Kernel_Multitasking).
 * This kernel has no user/kernel privilege split and no per-task
 * address space yet -- every task here runs in ring 0 sharing the one
 * page directory Chapter 8 built -- so this TCB carries nothing for
 * CR3 or a TSS.ESP0 field the way a fuller kernel's would; `esp` is
 * almost the whole of what switch_task() needs. `entry` is new since
 * Chapter 12 -- see task_start_trampoline() below for why. `state`
 * replaces this struct's old plain `done` flag this chapter: a task
 * can now be BLOCKED (waiting on a semaphore, see 032_semaphore.c)
 * as well as READY or DONE, and the scheduler needs to tell all three
 * apart, not just "finished or not." */
typedef enum {
    TASK_STATE_READY = 0,
    TASK_STATE_BLOCKED,
    TASK_STATE_DONE
} task_state_t;

struct task {
    uint32_t esp;        /* valid only while this task is NOT current */
    void *stack_base;     /* the kmalloc()'d block backing this task's
                            * stack -- kept only so a later chapter with
                            * real task teardown could kfree() it; this
                            * chapter never frees a task's stack */
    void (*entry)(void);  /* this task's real entry point -- kept so
                            * task_start_trampoline() can find it */
    task_state_t state;
    int is_usermode;         /* new this chapter: non-zero if this task
                               * runs its real `entry` at CPL 3, via
                               * enter_usermode(), rather than calling it
                               * directly at CPL 0 */
    uint32_t kernel_stack_top; /* new this chapter: only meaningful when
                               * is_usermode -- the top of this task's
                               * OWN dedicated kernel stack, the same
                               * stack `esp` above lives on. task_yield()
                               * loads this into the TSS's ESP0 field
                               * (032_tss.c) immediately before switching
                               * into this exact task, so any interrupt
                               * or syscall this task later raises at
                               * CPL 3 lands on ITS OWN kernel stack, not
                               * whichever task happened to run before
                               * it (OSDev Wiki, "Kernel Multitasking":
                               * "Adjust the ESP0 field in the TSS (used
                               * by CPU for CPL=3 -> CPL=0 privilege
                               * level changes)"). */
    uint32_t user_stack_top;  /* only meaningful when is_usermode -- the
                               * top of this task's own real,
                               * PAGE_USER-marked stack frame, passed to
                               * enter_usermode() the one time this
                               * task's trampoline actually starts it
                               * running at CPL 3. Since Chapter 18, this
                               * is always PROCESS_STACK_VADDR + 4096 --
                               * the same fixed virtual number for every
                               * process, backed by a different physical
                               * frame each time (see page_directory_phys
                               * below) -- kept as its own field anyway
                               * so task_start_trampoline() does not need
                               * to know that fact itself. */
    uint32_t page_directory_phys; /* new this chapter: 0 for every
                               * ring-0-only task (task_create()) and
                               * for task 0 -- meaning "whatever this
                               * kernel's own default/kernel directory
                               * currently is." Non-zero for a process
                               * (task_create_process()): the physical
                               * address of a page directory that exists
                               * nowhere else, built by
                               * paging_new_address_space() and populated
                               * only with this ONE process's own private
                               * code/stack mappings on top of the
                               * kernel's own shared ones. task_yield()
                               * reloads CR3 to this value immediately
                               * before switching into this task, exactly
                               * mirroring how it already reloads
                               * TSS.ESP0 for a ring-3 task since Chapter
                               * 16 -- see task_yield() below. */
};

/* Chapter 17 needed 12 slots (kmain's own task 0, Chapter 12's own
 * Task A/Task B, Chapter 13's own Stress A/Stress B, Chapter 14's own
 * two producers/two consumers, and Chapter 17's own three demo
 * processes -- Process A, Process B, Process Reckless). This chapter's
 * own demo replaces those three with two real ELF-loaded processes
 * (Process A, Process B, below) -- one fewer than Chapter 17 needed.
 * This scheduler still never reuses a finished task's slot, so the
 * fixed table has to be sized for every task any chapter's own
 * kmain() creates across a single boot, not just however many are
 * runnable at once: 1 (task 0) + 2 (Task A/B) + 2 (Stress A/B) + 4
 * (2 producers + 2 consumers) + 2 (ELF Process A/B) = 11. */
#define TASK_MAX_TASKS   11u
#define TASK_STACK_SIZE  4096u

static struct task tasks[TASK_MAX_TASKS];
static int task_count = 0;
static int current_task = 0;
static uint32_t switch_count = 0;
static int tasking_ready = 0;

/* New this chapter: the two page directories task_yield() has to
 * choose between on every single switch. kernel_page_directory_phys
 * is set once, in task_init(), by reading back whatever paging_init()
 * (Chapter 8) already loaded into CR3 -- this kernel's own default
 * directory, shared by task 0 and every ring-0-only task.
 * current_page_directory_phys tracks whichever directory CR3 ACTUALLY
 * holds right now, so task_yield() can skip a reload -- and the real
 * TLB flush a CR3 write causes -- whenever the task being switched
 * into already shares the address space that is already active,
 * exactly the same real optimization Chapter 16's own cited OSDev
 * source already applies to TSS.ESP0's own sibling field. */
static uint32_t kernel_page_directory_phys = 0;
static uint32_t current_page_directory_phys = 0;

extern void switch_task(uint32_t *old_esp_ptr, uint32_t new_esp);

/* 032_usermode.asm's real ring 0 -> ring 3 transition -- Chapter 15's
 * own real, hand-built IRET. Called from task_start_trampoline() below
 * instead of `entry()` directly, whenever the task starting up is a
 * ring-3 one. Never returns in the ordinary sense: the task it starts
 * keeps running at CPL 3 (and, eventually, calls a real SYS_EXIT
 * syscall) independently of whatever called it. */
extern void enter_usermode(void (*entry)(void), void *user_stack_top);

/* Chapter 11's task_create() put `entry` directly on a new task's
 * stack as the address switch_task()'s own `ret` would jump to --
 * correct for a purely cooperative scheduler, where every switch_task()
 * call happens with IF already 1 (interrupts enabled) throughout,
 * since nothing in that chapter ever touched IF at all. This chapter
 * changes that: every real switch now happens from *inside* an IRQ0
 * interrupt handler, entered through a real interrupt gate that the
 * CPU itself clears IF for on entry (OSDev Wiki's own IDT gate-type
 * description; this book's own idt_set_gate() calls have used type
 * 0x8E, a 32-bit interrupt gate, since Chapter 4). switch_task()
 * itself never saves or restores EFLAGS -- IF is one single, real,
 * shared CPU flag, not per-task state -- so whichever task gets
 * switched into next inherits whatever IF happens to be at that exact
 * moment: 0, because we are still inside that interrupt handler. A
 * task resuming through task_yield()'s own return point can restore
 * that itself (see below); but a task running for the very first
 * time jumps straight to `entry`, which has no such checkpoint to
 * pass through -- so without this trampoline, a brand-new task would
 * start running with interrupts silently, permanently disabled, and
 * this whole chapter's preemption would only ever work once. */
static void task_start_trampoline(void) {
    __asm__ volatile ("sti");

    /* New this chapter: a ring-3 task's real entry point cannot simply
     * be called like an ordinary C function -- it has to be started
     * through a real CPL 0 -> CPL 3 transition (032_usermode.asm), on
     * its own dedicated user stack, not this task's kernel stack.
     * enter_usermode() never returns: this task keeps running at CPL 3
     * until its own real SYS_EXIT syscall (032_isr_handlers.c) calls
     * task_exit() directly, from ring 0, on this exact task's own
     * kernel stack -- so the task_exit() call below is never reached
     * for a ring-3 task at all. */
    if (tasks[current_task].is_usermode) {
        enter_usermode(tasks[current_task].entry,
                        (void *) (uintptr_t) tasks[current_task].user_stack_top);
    } else {
        tasks[current_task].entry();
    }

    /* Defensive, not load-bearing: every real ring-0 task body in this
     * chapter already calls task_exit() itself. If one ever forgot,
     * this is what stands between that mistake and running off the
     * end of a kmalloc()'d stack into whatever memory happens to
     * follow it. */
    task_exit();
}

void task_init(void) {
    tasks[0].esp = 0;         /* never read while task 0 is current */
    tasks[0].stack_base = 0;   /* task 0 owns no kmalloc'd stack -- it
                                 * is this kernel's own boot stack,
                                 * already running before task_init()
                                 * is ever called */
    tasks[0].entry = 0;         /* task 0 never starts via the
                                  * trampoline -- it is already running */
    tasks[0].state = TASK_STATE_READY;
    tasks[0].is_usermode = 0;   /* task 0 is this kernel's own boot
                                  * task -- always ring 0 */
    tasks[0].kernel_stack_top = 0;
    tasks[0].user_stack_top = 0;
    tasks[0].page_directory_phys = 0;  /* runs in the kernel's own
                                         * default directory, like
                                         * every ring-0-only task */
    task_count = 1;
    current_task = 0;
    switch_count = 0;
    tasking_ready = 1;

    /* This kernel's own default page directory is simply whatever CR3
     * already holds the moment task_init() runs -- paging_init() (Ch8)
     * always loads it well before this point in kmain(), so reading it
     * back here, once, is enough to remember it for the rest of this
     * kernel's life. task_yield() reloads CR3 to this exact value
     * whenever switching INTO a task whose own page_directory_phys is
     * 0 -- the counterpart of reloading it to a process's own directory
     * when switching into one of those instead (see task_yield()). */
    __asm__ volatile ("mov %%cr3, %0" : "=r" (kernel_page_directory_phys));
    current_page_directory_phys = kernel_page_directory_phys;
}

int task_create(void (*entry)(void)) {
    if (task_count >= (int) TASK_MAX_TASKS) {
        kprintf("task_create: task table full (%u tasks) -- refusing\n", TASK_MAX_TASKS);
        return -1;
    }

    struct task *t = &tasks[task_count];
    void *stack = kmalloc(TASK_STACK_SIZE);
    uint32_t top = (uint32_t) (uintptr_t) stack + TASK_STACK_SIZE;
    uint32_t *sp = (uint32_t *) top;

    /* Build the exact stack frame switch_task()'s own epilogue expects
     * to pop the first time this task is switched into: EDI, ESI, EBX,
     * EBP (in that order, since switch_task() pushed EBP first and
     * pops in reverse), then a return address on top -- which is what
     * makes switch_task()'s final `ret` land directly on
     * task_start_trampoline (OSDev Wiki, "X86 Cooperative Multitasking
     * Tutorial": a new task's registers are initialized with
     * "task->regs.esp = (uint32_t) allocPage() + 0x1000" and
     * "task->regs.eip = (uint32_t) main"). Zero is a fine placeholder
     * for EBP/EBX/ESI/EDI here -- nothing has run inside this task yet
     * to give those registers a real saved value. */
    *(--sp) = (uint32_t) (uintptr_t) task_start_trampoline;  /* return address for `ret` */
    *(--sp) = 0;  /* ebp */
    *(--sp) = 0;  /* ebx */
    *(--sp) = 0;  /* esi */
    *(--sp) = 0;  /* edi */

    t->esp = (uint32_t) (uintptr_t) sp;
    t->stack_base = stack;
    t->entry = entry;
    t->state = TASK_STATE_READY;
    t->is_usermode = 0;
    t->kernel_stack_top = 0;
    t->user_stack_top = 0;
    t->page_directory_phys = 0;  /* runs in the kernel's own default
                                   * directory */

    int index = task_count;
    task_count++;
    return index;
}

int task_create_process(const void *template_start, uint32_t template_size) {
    if (task_count >= (int) TASK_MAX_TASKS) {
        kprintf("task_create_process: task table full (%u tasks) -- refusing\n", TASK_MAX_TASKS);
        return -1;
    }
    if (template_size > 4096u) {
        /* This chapter's own real templates are each well under one
         * page; refusing rather than silently truncating a future,
         * larger one is the same honest-by-default choice this book
         * has made everywhere else a real size limit exists. */
        kprintf("task_create_process: template is %u bytes, larger than one page -- refusing\n",
                template_size);
        return -1;
    }

    struct task *t = &tasks[task_count];

    /* This task's own dedicated kernel stack -- built exactly like
     * task_create()'s own ordinary task stack above (same size, same
     * pre-populated switch_task() frame landing on
     * task_start_trampoline), because it plays the exact same role for
     * switch_task() itself, AND is this task's own TSS.ESP0 target the
     * instant it becomes current (see task_yield() below) -- one real
     * stack, two real jobs, never shared with any other task. Built
     * BEFORE this task's own new address space, deliberately: this
     * kmalloc() call may itself grow the kheap (032_kheap.c), and
     * paging_new_address_space() below needs to copy the kernel's own
     * directory AFTER any such growth has already happened, or this
     * task's own copy would be missing it. */
    void *kstack = kmalloc(TASK_STACK_SIZE);
    uint32_t kstack_top = (uint32_t) (uintptr_t) kstack + TASK_STACK_SIZE;
    uint32_t *sp = (uint32_t *) kstack_top;

    *(--sp) = (uint32_t) (uintptr_t) task_start_trampoline;  /* return address for `ret` */
    *(--sp) = 0;  /* ebp */
    *(--sp) = 0;  /* ebx */
    *(--sp) = 0;  /* esi */
    *(--sp) = 0;  /* edi */

    /* This task's own real, private address space -- the whole reason
     * this function exists. Every process gets a genuinely separate
     * page directory, sharing the kernel's own mappings (see
     * paging_new_address_space()'s own comment) but with room for
     * this ONE process's own private code and stack that no other
     * process's directory will ever contain. */
    uint32_t dir_phys = paging_new_address_space();

    /* This process's own private copy of its code: a fresh physical
     * frame, with `template_size` real bytes copied into it from the
     * kernel image's own template function -- and ONLY then mapped
     * into this task's OWN new directory at the fixed virtual address
     * every process uses for its code. Two different processes'
     * PROCESS_CODE_VADDR therefore resolve to two different real
     * physical frames -- 032_kmain.c's own kmain() proves this
     * directly, with paging_translate_in(), before either process
     * ever runs. No RW bit: this range is only ever fetched from. */
    uint32_t code_frame = pmm_alloc_frame();
    const uint8_t *src = (const uint8_t *) template_start;
    uint8_t *dst = (uint8_t *) (uintptr_t) code_frame;
    for (uint32_t i = 0; i < template_size; i++) {
        dst[i] = src[i];
    }
    paging_map_page_in(dir_phys, PROCESS_CODE_VADDR, code_frame, PAGE_PRESENT | PAGE_USER);

    /* This process's own private user stack: one fresh frame, mapped
     * only into this task's own directory, at the fixed virtual
     * address every process uses for its stack. */
    uint32_t stack_frame = pmm_alloc_frame();
    paging_map_page_in(dir_phys, PROCESS_STACK_VADDR, stack_frame, PAGE_PRESENT | PAGE_RW | PAGE_USER);

    t->esp = (uint32_t) (uintptr_t) sp;
    t->stack_base = kstack;
    t->entry = (void (*)(void)) (uintptr_t) PROCESS_CODE_VADDR;
    t->state = TASK_STATE_READY;
    t->is_usermode = 1;
    t->kernel_stack_top = kstack_top;
    t->user_stack_top = PROCESS_STACK_VADDR + 4096u;
    t->page_directory_phys = dir_phys;

    int index = task_count;
    task_count++;
    return index;
}

int task_create_elf_process(uint32_t module_start, uint32_t module_end) {
    if (task_count >= (int) TASK_MAX_TASKS) {
        kprintf("task_create_elf_process: task table full (%u tasks) -- refusing\n", TASK_MAX_TASKS);
        return -1;
    }

    struct task *t = &tasks[task_count];

    /* Identical reasoning and identical order to task_create_process()
     * above: this task's own dedicated kernel stack is built FIRST,
     * before its own new address space, so any kheap growth that
     * kmalloc() call causes is already reflected in the directory
     * paging_new_address_space() copies below. */
    void *kstack = kmalloc(TASK_STACK_SIZE);
    uint32_t kstack_top = (uint32_t) (uintptr_t) kstack + TASK_STACK_SIZE;
    uint32_t *sp = (uint32_t *) kstack_top;

    *(--sp) = (uint32_t) (uintptr_t) task_start_trampoline;  /* return address for `ret` */
    *(--sp) = 0;  /* ebp */
    *(--sp) = 0;  /* ebx */
    *(--sp) = 0;  /* esi */
    *(--sp) = 0;  /* edi */

    uint32_t dir_phys = paging_new_address_space();

    /* The one real difference from task_create_process(): this task's
     * own code is not a fixed number of bytes copied to a fixed
     * kernel-chosen virtual address at all. 032_elf.c's own elf_load()
     * walks the module's real ELF program headers and maps however
     * many real PT_LOAD segments the FILE ITSELF describes, at
     * whatever real virtual addresses the file's own p_vaddr fields
     * specify -- and hands back the file's own real entry point,
     * e_entry, which is what this task actually starts running at. If
     * the module fails to parse as a valid ELF32 executable, this
     * function refuses outright rather than starting a task with no
     * real entry point to run. */
    uint32_t entry_vaddr;
    if (!elf_load(dir_phys, module_start, module_end, &entry_vaddr)) {
        kprintf("task_create_elf_process: elf_load() failed -- refusing to create this task\n");
        return -1;
    }

    /* This task's own private user stack: same fixed convention as
     * task_create_process() -- the stack is this kernel's own concern,
     * independent of anything the loaded ELF file itself specifies. */
    uint32_t stack_frame = pmm_alloc_frame();
    paging_map_page_in(dir_phys, PROCESS_STACK_VADDR, stack_frame, PAGE_PRESENT | PAGE_RW | PAGE_USER);

    t->esp = (uint32_t) (uintptr_t) sp;
    t->stack_base = kstack;
    t->entry = (void (*)(void)) (uintptr_t) entry_vaddr;
    t->state = TASK_STATE_READY;
    t->is_usermode = 1;
    t->kernel_stack_top = kstack_top;
    t->user_stack_top = PROCESS_STACK_VADDR + 4096u;
    t->page_directory_phys = dir_phys;

    int index = task_count;
    task_count++;
    return index;
}

uint32_t task_page_directory_phys(int index) {
    return tasks[index].page_directory_phys;
}

void task_yield(void) {
    /* OSDev Wiki, "Kernel Multitasking": "Caller is expected to
     * disable IRQs before calling, and enable IRQs again after
     * function returns" -- rather than push that requirement onto
     * every caller, task_yield() enforces it itself, so it stays
     * correct regardless of whether it was reached from an ordinary
     * cdecl call or, as every real call in this chapter's own run
     * is, from inside an interrupt handler where IF is already 0. */
    __asm__ volatile ("cli");

    int old = current_task;
    int next = -1;

    /* Scans every OTHER task first, in round-robin order, then wraps
     * all the way back around to `old` itself as the last candidate
     * checked. This one loop now correctly covers every case this
     * chapter's own three task states can produce: some other READY
     * task exists (ordinary switch); no other task is READY but `old`
     * itself still is (the loop's own wraparound finds it, and the
     * "next == old" check below turns that into a no-op); or `old`
     * itself is no longer READY either -- freshly BLOCKED by this
     * chapter's own task_block_self(), or DONE -- in which case the
     * whole scan finds nothing at all and `next` stays -1. Chapter
     * 13's own version of this loop only ever had to handle the first
     * two cases, since nothing in that chapter could make `old` itself
     * non-runnable out from under its own yield. */
    for (int i = 1; i <= task_count; i++) {
        int candidate = (old + i) % task_count;
        if (tasks[candidate].state == TASK_STATE_READY) {
            next = candidate;
            break;
        }
    }

    if (next == -1) {
        /* Nothing in the whole task table is READY -- not some other
         * task, and not even this one. Every earlier chapter's own
         * version of this message meant "everyone has called
         * task_exit()"; this chapter adds a second real way to reach
         * it: every remaining task is genuinely BLOCKED on a
         * semaphore that nothing will ever signal. Both are real dead
         * ends this scheduler has no idle task to fall back on for. */
        kprintf("task_yield: no runnable task remains -- halting\n");
        for (;;) {
            __asm__ volatile ("hlt");
        }
    }

    if (next == old) {
        __asm__ volatile ("sti");
        return;  /* only this task is runnable -- nothing to switch to */
    }

    current_task = next;
    switch_count++;

    /* New this chapter: whenever the task we are about to switch INTO
     * is a ring-3 one, its own dedicated kernel stack has to already
     * be installed as the CPU's real ESP0 target BEFORE switch_task()
     * ever runs -- otherwise the very next interrupt or syscall this
     * task raises at CPL 3 (which can happen at any point after this
     * function returns, including immediately) would land on whichever
     * OTHER task's kernel stack the TSS still happened to point at
     * (OSDev Wiki, "Kernel Multitasking": "Adjust the ESP0 field in
     * the TSS (used by CPU for CPL=3 -> CPL=0 privilege level
     * changes)"). This covers both a ring-3 task's very first
     * activation (landing in task_start_trampoline for the first time)
     * and every later resume identically -- tasks[next].esp already
     * points at the right place either way; only the CPU's own ESP0
     * register needs updating here. Ring-0-only tasks never trigger a
     * CPL change at all, so this is skipped for them entirely -- the
     * TSS's ESP0 field is simply irrelevant while one is running. */
    if (tasks[next].is_usermode) {
        tss_set_kernel_stack(tasks[next].kernel_stack_top);
    }

    /* New this chapter: reload CR3 whenever the task being switched
     * INTO belongs to a different address space than the one already
     * active -- cited directly from the very same OSDev Wiki example
     * Chapter 16 already used for ESP0 above: "cmp eax,ecx ; Does the
     * virtual address space need to being changed? je .doneVAS ; no,
     * ... so don't reload it and cause TLB flushes ; mov cr3,eax ;
     * yes, load the next task's virtual address space" (OSDev Wiki,
     * "Kernel Multitasking"). `target_dir` is that task's own private
     * directory for a process, or the kernel's own default directory
     * for every ring-0-only task (page_directory_phys == 0) -- so a
     * switch between two ordinary ring-0 tasks, or into task 0, always
     * resolves to the SAME kernel_page_directory_phys value and
     * correctly skips the reload, exactly like every chapter before
     * this one already did implicitly, by never touching CR3 at all. */
    uint32_t target_dir = (tasks[next].page_directory_phys != 0)
                               ? tasks[next].page_directory_phys
                               : kernel_page_directory_phys;
    if (target_dir != current_page_directory_phys) {
        __asm__ volatile ("mov %0, %%cr3" : : "r" (target_dir) : "memory");
        current_page_directory_phys = target_dir;
    }

    switch_task(&tasks[old].esp, tasks[next].esp);

    /* Execution only reaches here once this exact task is switched
     * back into again -- possibly much later, and possibly by
     * unwinding all the way back up through some real IRQ0 stub's own
     * `iret`, which is a second, independent place IF=1 also gets
     * restored from. The explicit `sti` here is still required: it is
     * what re-enables interrupts for every task that resumes through
     * this exact return point rather than through an `iret` at all. */
    __asm__ volatile ("sti");
}

void task_exit(void) {
    tasks[current_task].state = TASK_STATE_DONE;
    task_yield();
    /* unreachable: task_yield() above either switches this task's own
     * stack away permanently (this call frame is simply abandoned,
     * never resumed again) or halts the kernel outright if nothing
     * else is runnable. */
}

int task_is_done(int index) {
    return tasks[index].state == TASK_STATE_DONE;
}

int task_current_id(void) {
    return current_task;
}

void task_block_self(void) {
    tasks[current_task].state = TASK_STATE_BLOCKED;
}

void task_wake(int index) {
    tasks[index].state = TASK_STATE_READY;
}

uint32_t task_switch_count(void) {
    return switch_count;
}

void task_tick(void) {
    if (!tasking_ready) {
        return;  /* task_init() has not run yet -- nothing to preempt */
    }
    task_yield();
}
