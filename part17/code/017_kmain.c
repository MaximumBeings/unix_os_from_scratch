/* Chapter 16 made ring-3 code a real, first-class participant of this
 * kernel's own scheduler -- but every ring-3 task it ever created,
 * Ring3 Task A and Ring3 Task B included, still shared this kernel's
 * ONE page directory. Genuinely running at CPL 3 kept either task
 * from executing a privileged instruction, but nothing kept one from
 * reaching straight into the other's own memory: every ring-3
 * mapping lived in the same one shared address space, so isolation
 * was a matter of neither task happening to try, not of the hardware
 * refusing it.
 *
 * This chapter gives each ring-3 task -- a real PROCESS, the word
 * this book uses from here on -- its own private page directory
 * instead. 017_paging.c's new paging_new_address_space() builds one
 * by copying every one of the kernel's own 1024 real page-directory
 * entries, so every process still shares the kernel's own code, heap
 * and drivers, but each gets EXCLUSIVE, private mappings of its own
 * on top. 017_task.c's new task_create_process() uses it to give
 * every process the exact same two fixed virtual addresses for its
 * own code and stack (017_task.h's PROCESS_CODE_VADDR/
 * PROCESS_STACK_VADDR), each backed by a different, private physical
 * frame per process -- and task_yield() now reloads CR3 on every
 * switch into a process, exactly mirroring how it has already
 * reloaded TSS.ESP0 since Chapter 16 (OSDev Wiki, "Kernel
 * Multitasking," the same real example already cited there: "mov
 * cr3,eax ; yes, load the next task's virtual address space" --
 * "cmp eax,ecx ... je .doneVAS ; no, virtual address space is the
 * same, so don't reload it and cause TLB flushes").
 *
 * This chapter's own demo below creates two real processes -- Process
 * A and Process B -- running the SAME template function at the SAME
 * fixed virtual address, and proves from ring 0, with
 * paging_translate_in(), that address resolves to two different real
 * physical frames in the two processes' own directories. A third
 * process, Process Reckless, then deliberately reads an address
 * mapped in NEITHER process's own directory, to capture a genuinely
 * new flavor of page fault this book has not shown before: not
 * "non-present, kernel, write" (Chapter 10) and not "protection
 * violation, user, read" (Chapter 15), but "non-present, user, read"
 * -- the hardware refusing a process's OWN attempt to reach memory it
 * was simply never given. */

#include <stdint.h>

#include "017_gdt.h"
#include "017_idt.h"
#include "017_keyboard.h"
#include "017_kheap.h"
#include "017_multiboot.h"
#include "017_paging.h"
#include "017_pic.h"
#include "017_pit.h"
#include "017_pmm.h"
#include "017_printf.h"
#include "017_semaphore.h"
#include "017_serial.h"
#include "017_spinlock.h"
#include "017_syscall.h"
#include "017_task.h"
#include "017_vga.h"

#define TIMER_FREQUENCY_HZ 100

/* Deliberately far outside the 0-64 MiB identity-mapped range (which
 * only ever occupies page-directory entries 0-15): 0xC0000000 >> 22 =
 * 768. Mapping here forces paging_map_page() to allocate a genuinely
 * new page table rather than reusing one paging_init() already built. */
#define TEST_VIRT_ADDR 0xC0000000u

/* Defined by 017_linker.ld, not by this file -- the linker is the one
 * part of this toolchain that genuinely knows where the kernel image's
 * last real section ends in physical memory. */
extern char kernel_end[];

/* How much real, uninterruptible-looking work each task does before
 * it naturally finishes -- large enough that many real IRQ0 ticks (at
 * 100 Hz, one every ~10 ms) land somewhere in the middle of it, since
 * a single pass through this loop takes QEMU's emulated CPU far less
 * than 10 ms. Chosen empirically from this chapter's own real run,
 * the same way every prior chapter's own real constants were. */
#define TASK_WORK_TARGET 4000000u
#define TASK_PRINT_EVERY   500000u

/* How many kmalloc()/kfree() round trips each stress task performs.
 * Chosen empirically from this chapter's own real runs: large enough
 * that, at 100 real IRQ0 ticks per second, many ticks land somewhere
 * in the middle of the whole run -- and therefore stand a real chance
 * of landing inside kmalloc()'s or kfree()'s own free-list
 * manipulation, not just between two whole calls. */
#define STRESS_ITERATIONS  3000000u
#define STRESS_PRINT_EVERY  500000u

/* This chapter's two demo tasks. Neither one calls task_yield()
 * anywhere in this loop -- the whole point. Whatever interleaving
 * this chapter's real run shows is forced entirely by the real timer,
 * not requested by either task. */
static void task_a_entry(void) {
    for (uint32_t i = 1; i <= TASK_WORK_TARGET; i++) {
        if (i % TASK_PRINT_EVERY == 0) {
            kprintf("  Task A: %u\n", i);
        }
    }
    kprintf("  Task A: done\n");
    task_exit();
}

static void task_b_entry(void) {
    for (uint32_t i = 1; i <= TASK_WORK_TARGET; i++) {
        if (i % TASK_PRINT_EVERY == 0) {
            kprintf("  Task B: %u\n", i);
        }
    }
    kprintf("  Task B: done\n");
    task_exit();
}

/* This chapter's real evidence tasks: two preemptible tasks racing on
 * kmalloc()/kfree() with no synchronization between them at all. Each
 * one only ever touches its own pointer, one allocation at a time --
 * any corruption that shows up is entirely the free list's own doing,
 * not a bug in either task's own logic. */
static void stress_task_a_entry(void) {
    for (uint32_t i = 1; i <= STRESS_ITERATIONS; i++) {
        void *p = kmalloc(32);
        if (p != 0) {
            *(volatile uint8_t *) p = 0xAA;
        }
        kfree(p);
        if (i % STRESS_PRINT_EVERY == 0) {
            kprintf("  Stress A: %u\n", i);
        }
    }
    kprintf("  Stress A: done\n");
    task_exit();
}

static void stress_task_b_entry(void) {
    for (uint32_t i = 1; i <= STRESS_ITERATIONS; i++) {
        void *p = kmalloc(64);
        if (p != 0) {
            *(volatile uint8_t *) p = 0xBB;
        }
        kfree(p);
        if (i % STRESS_PRINT_EVERY == 0) {
            kprintf("  Stress B: %u\n", i);
        }
    }
    kprintf("  Stress B: done\n");
    task_exit();
}

/* This chapter's own demo: a classic bounded-buffer producer/consumer,
 * built on this chapter's new semaphores plus Chapter 13's own
 * spinlock. `sem_empty_slots` starts at BUFFER_CAPACITY (that many
 * slots are free right now) and `sem_full_slots` starts at 0 (nothing
 * produced yet) -- the two together are what make a producer block
 * when the buffer is genuinely full and a consumer block when it is
 * genuinely empty, without either one ever spinning to find out. The
 * buffer's own read/write indices are a separate, much shorter
 * critical section, protected by an ordinary spinlock -- exactly the
 * kind of short, bounded update Chapter 13's spinlock is for. */
#define BUFFER_CAPACITY     4u
#define ITEMS_PER_PRODUCER 15u
#define ITEMS_PER_CONSUMER 15u

static int shared_buffer[BUFFER_CAPACITY];
static uint32_t buffer_write_idx = 0;
static uint32_t buffer_read_idx = 0;
static spinlock_t buffer_lock;
static semaphore_t sem_empty_slots;
static semaphore_t sem_full_slots;

static void produce(const char *label, uint32_t item_base) {
    for (uint32_t i = 1; i <= ITEMS_PER_PRODUCER; i++) {
        int item = (int) (item_base + i);

        /* Blocks for real if the buffer is already full -- this is
         * the whole point of this chapter, not busy-waiting. */
        semaphore_wait(&sem_empty_slots);

        uint32_t saved_eflags = spinlock_acquire(&buffer_lock);
        shared_buffer[buffer_write_idx] = item;
        buffer_write_idx = (buffer_write_idx + 1) % BUFFER_CAPACITY;
        spinlock_release(&buffer_lock, saved_eflags);

        semaphore_signal(&sem_full_slots);
        kprintf("  %s: produced %d\n", label, item);
    }
    kprintf("  %s: done\n", label);
    task_exit();
}

static void consume(const char *label) {
    for (uint32_t i = 1; i <= ITEMS_PER_CONSUMER; i++) {
        /* Blocks for real if the buffer is empty -- the mirror image
         * of produce()'s own semaphore_wait() above. */
        semaphore_wait(&sem_full_slots);

        uint32_t saved_eflags = spinlock_acquire(&buffer_lock);
        int item = shared_buffer[buffer_read_idx];
        buffer_read_idx = (buffer_read_idx + 1) % BUFFER_CAPACITY;
        spinlock_release(&buffer_lock, saved_eflags);

        semaphore_signal(&sem_empty_slots);
        kprintf("  %s: consumed %d\n", label, item);
    }
    kprintf("  %s: done\n", label);
    task_exit();
}

static void producer_a_entry(void) { produce("Producer A", 0u); }
static void producer_b_entry(void) { produce("Producer B", 100u); }
static void consumer_a_entry(void) { consume("Consumer A"); }
static void consumer_b_entry(void) { consume("Consumer B"); }

/* How many times Process A and Process B each print and then
 * SYS_YIELD before their own real SYS_EXIT. Deliberately small: a
 * real, independently-checkable lower bound on this chapter's own
 * final switch count falls straight out of this one constant (see
 * kmain()'s own comment on it below), the same way Chapter 12's own
 * TIMER_FREQUENCY_HZ made that chapter's tick count independently
 * checkable. */
#define PROCESS_TASK_ITERATIONS 5u

/* The one virtual address Process Reckless deliberately reads that is
 * mapped in NO process's own directory -- not the kernel's shared
 * range, and not either PROCESS_CODE_VADDR/PROCESS_STACK_VADDR
 * mapping either, since those are private per process and Process
 * Reckless has its own. Chosen inside the same 0xE8xxxxxx neighborhood
 * as 017_task.h's own two process addresses purely so a reader
 * scanning this file sees at a glance that all three belong to this
 * chapter's one new address range, well clear of every earlier
 * chapter's own special addresses. */
#define UNMAPPED_PROBE_VADDR 0xE8003000u

/* This chapter's two per-process template functions -- the entire
 * reason 017_task.c's new task_create_process() exists, and the
 * reason this file no longer has a shared usermode_syscall() helper
 * the way Chapter 16 did. task_create_process() does not run this
 * code in place: it physically COPIES the whole compiled function,
 * byte for byte, out of its own dedicated page-aligned linker section
 * (017_linker.ld) into a brand-new physical frame, and maps that COPY
 * at the fixed virtual address PROCESS_CODE_VADDR -- a different
 * address than wherever the compiler actually placed this function in
 * the kernel image. A CALL or JMP to code that did NOT get copied
 * alongside would still carry the PC-relative displacement the
 * compiler computed for the ORIGINAL address, which is simply wrong
 * once the copy runs somewhere else -- so every syscall here is an
 * inlined `int $0x80` sequence, not a call to a shared helper.
 * Internal control flow (this function's own loop) stays correct
 * because x86 near jumps and branches are PC-relative to begin with,
 * and the string literal each one prints stays correct too: only the
 * CODE moved, never the kernel's own .rodata that holds the string,
 * and every process's directory shares that mapping (paging_new_
 * address_space() copies all 1024 real page-directory entries, .rodata
 * included) -- exactly the same reasoning Chapter 16's own ring-3
 * tasks already relied on for their message strings. Each function
 * must fit in the one page its own linker section reserves. */
/* `used` alongside `section`: nothing in this translation unit ever
 * calls this function through an ordinary C call -- only through the
 * linker-defined process_template_normal_start symbol, copied byte for
 * byte by task_create_process() -- so without it, GCC's own dead-code
 * warning would flag a real, live piece of this kernel as unused. */
__attribute__((section(".process_template_normal"), used))
static void process_template_normal(void) {
    const char *msg = "  printed via a real SYS_WRITE_STR, now yielding via a real SYS_YIELD\n";
    for (uint32_t i = 1; i <= PROCESS_TASK_ITERATIONS; i++) {
        register uint32_t sys_num asm("eax") = SYS_WRITE_STR;
        register uint32_t sys_arg asm("ebx") = (uint32_t) (uintptr_t) msg;
        __asm__ volatile ("int $0x80" :: "r" (sys_num), "r" (sys_arg) : "memory");

        register uint32_t yield_num asm("eax") = SYS_YIELD;
        register uint32_t yield_arg asm("ebx") = 0u;
        __asm__ volatile ("int $0x80" :: "r" (yield_num), "r" (yield_arg) : "memory");
    }

    register uint32_t exit_num asm("eax") = SYS_EXIT;
    register uint32_t exit_arg asm("ebx") = 0u;
    __asm__ volatile ("int $0x80" :: "r" (exit_num), "r" (exit_arg) : "memory");

    /* Unreachable in this chapter's own real run: SYS_EXIT's own
     * isr128_handler() case calls task_exit() directly, which never
     * returns (017_task.c) -- this process's entire kernel stack,
     * including this exact call frame, is simply abandoned at that
     * point. HLT would itself be a second privileged instruction this
     * process has no right to execute at CPL 3 (exactly like Chapter
     * 15's own deliberate CLI demo), so this defensive tail is a
     * plain empty spin, never CLI or HLT. */
    for (;;) { }
}

/* Process Reckless: one normal print-and-yield cycle, identical to
 * process_template_normal()'s own first iteration, then a single,
 * deliberate read of UNMAPPED_PROBE_VADDR -- an address this process's
 * own directory (like every other process's own directory) has never
 * mapped at all. 017_paging.c's page-fault handler (017_isr_handlers.c,
 * carried forward unchanged from Chapter 15) reads CR2 and the real
 * hardware error code to report exactly which flavor of fault this
 * is; OSDev Wiki, "Paging": bit 0 of that error code is 0 for "the
 * fault was caused by a non-present page" and 1 for "the fault was
 * caused by a page-protection violation." This chapter's own real
 * captured run is the first in this book to show bit 0 clear (a
 * genuinely UNMAPPED address) together with bit 2 set (a USER-mode
 * access) -- Chapter 10's own #PF was non-present but from
 * supervisor code (017_kmain.c's TEST_VIRT_ADDR probe before it was
 * mapped), and Chapter 15's own #PF was user-mode but a real
 * protection violation (writing a read-only page), never both
 * "genuinely unmapped" and "from ring 3" at once until now. */
__attribute__((section(".process_template_reckless"), used))
static void process_template_reckless(void) {
    const char *msg = "  Reckless: one normal cycle done, now reading an address "
                       "mapped in NO process's own directory...\n";
    register uint32_t sys_num asm("eax") = SYS_WRITE_STR;
    register uint32_t sys_arg asm("ebx") = (uint32_t) (uintptr_t) msg;
    __asm__ volatile ("int $0x80" :: "r" (sys_num), "r" (sys_arg) : "memory");

    register uint32_t yield_num asm("eax") = SYS_YIELD;
    register uint32_t yield_arg asm("ebx") = 0u;
    __asm__ volatile ("int $0x80" :: "r" (yield_num), "r" (yield_arg) : "memory");

    volatile uint8_t *probe = (volatile uint8_t *) UNMAPPED_PROBE_VADDR;
    volatile uint8_t value = *probe;
    (void) value;

    /* Unreachable in this chapter's own real run -- the read above
     * faults for real before execution ever reaches here. */
    for (;;) { }
}

void kmain(uint32_t magic, uint32_t mboot_info_addr) {
    serial_init();
    vga_init();
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);

    kprintf("Unix OS from Scratch -- Chapter 17: kernel entry reached\n");

    if (magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
        kprintf("FATAL: EAX held 0x%x at entry, not the real Multiboot2 magic 0x%x -- halting\n",
                magic, MULTIBOOT2_BOOTLOADER_MAGIC);
        __asm__ volatile ("cli");
        for (;;) { __asm__ volatile ("hlt"); }
    }
    kprintf("Multiboot2 magic confirmed in EAX: 0x%x\n", magic);

    const struct multiboot_tag_mmap *mmap = multiboot_find_mmap(mboot_info_addr);
    if (mmap == 0) {
        kprintf("FATAL: no memory map tag in this boot information structure -- halting\n");
        __asm__ volatile ("cli");
        for (;;) { __asm__ volatile ("hlt"); }
    }
    multiboot_print_mmap(mmap);

    uint32_t kernel_end_addr = (uint32_t) (uintptr_t) kernel_end;
    kprintf("Kernel image occupies physical 0x100000 - 0x%x\n", kernel_end_addr);

    pmm_init(mmap, 0x100000, kernel_end_addr);
    uint32_t free_frames = pmm_count_free_frames();
    kprintf("Physical memory manager ready: %u free frames (%u KiB usable)\n",
            free_frames, free_frames * 4);

    uint32_t f1 = pmm_alloc_frame();
    uint32_t f2 = pmm_alloc_frame();
    uint32_t f3 = pmm_alloc_frame();
    kprintf("Allocated three real frames: 0x%x, 0x%x, 0x%x\n", f1, f2, f3);

    pmm_free_frame(f2);
    kprintf("Freed the middle frame 0x%x -- %u free frames now\n", f2, pmm_count_free_frames());

    uint32_t f4 = pmm_alloc_frame();
    kprintf("Allocated again: got 0x%x (matches the freed frame? %s)\n",
            f4, (f4 == f2) ? "yes" : "no");

    paging_init();

    uint32_t test_frame = pmm_alloc_frame();
    paging_map_page(TEST_VIRT_ADDR, test_frame, PAGE_PRESENT | PAGE_RW);

    volatile uint32_t *via_virtual = (volatile uint32_t *) TEST_VIRT_ADDR;
    volatile uint32_t *via_identity = (volatile uint32_t *) test_frame;

    *via_virtual = 0xCAFEF00Du;
    kprintf("Wrote 0x%x through virtual address 0x%x\n", *via_virtual, TEST_VIRT_ADDR);
    kprintf("Reading the SAME physical frame (0x%x) through its identity-mapped address: 0x%x\n",
            test_frame, *via_identity);

    kheap_init();

    kprintf("kmalloc: three real allocations --\n");
    void *a = kmalloc(64);
    void *b = kmalloc(128);
    void *c = kmalloc(32);
    kprintf("  a=0x%x (64 bytes), b=0x%x (128 bytes), c=0x%x (32 bytes)\n",
            (uint32_t) (uintptr_t) a, (uint32_t) (uintptr_t) b, (uint32_t) (uintptr_t) c);
    kheap_dump();

    kfree(b);
    kprintf("kfree(b) -- middle block freed:\n");
    kheap_dump();

    void *d = kmalloc(128);
    kprintf("kmalloc(128) again: got 0x%x (matches freed b? %s)\n",
            (uint32_t) (uintptr_t) d, (d == b) ? "yes" : "no");
    kheap_dump();

    kfree(a);
    kfree(c);
    kfree(d);
    kprintf("Freed a, c, d -- coalesced back to one free block?\n");
    kheap_dump();

    kprintf("kmalloc(20000) -- larger than the whole initial 16 KiB heap, forcing real growth:\n");
    void *big = kmalloc(20000);
    kprintf("  big=0x%x (20000 bytes)\n", (uint32_t) (uintptr_t) big);
    kheap_dump();
    kfree(big);

    gdt_init();
    idt_init();
    pic_remap(0x20, 0x28);
    pic_disable_all();
    keyboard_init();
    pit_init(TIMER_FREQUENCY_HZ);

    kprintf("GDT/IDT/PIC/PIT/keyboard all initialized -- enabling interrupts now\n");
    __asm__ volatile ("sti");

    while (pit_get_ticks() < 200) {
        __asm__ volatile ("hlt");
    }
    kprintf("%u real IRQ0 ticks delivered -- interrupts confirmed still working.\n", pit_get_ticks());

    kprintf("\nStarting two real PREEMPTIVELY scheduled kernel tasks (Task A, Task B)...\n");
    kprintf("Neither task -- nor this wait loop -- ever calls task_yield() itself.\n");
    uint32_t ticks_before_tasks = pit_get_ticks();
    task_init();
    int task_a_id = task_create(task_a_entry);
    int task_b_id = task_create(task_b_entry);
    kprintf("task_create() returned id %d for Task A, id %d for Task B\n", task_a_id, task_b_id);

    while (!task_is_done(task_a_id) || !task_is_done(task_b_id)) {
        __asm__ volatile ("hlt");
    }

    uint32_t ticks_after_tasks = pit_get_ticks();
    kprintf("Both tasks finished -- %u real ticks elapsed, %u total real context switches\n",
            ticks_after_tasks - ticks_before_tasks, task_switch_count());

    kprintf("\nkheap before the stress test:\n");
    kheap_dump();

    kprintf("\nStarting Stress A and Stress B: %u kmalloc()/kfree() round trips each, "
            "racing on the SAME kheap free list with no synchronization...\n", STRESS_ITERATIONS);
    int stress_a_id = task_create(stress_task_a_entry);
    int stress_b_id = task_create(stress_task_b_entry);
    kprintf("task_create() returned id %d for Stress A, id %d for Stress B\n",
            stress_a_id, stress_b_id);

    while (!task_is_done(stress_a_id) || !task_is_done(stress_b_id)) {
        __asm__ volatile ("hlt");
    }

    kprintf("Both stress tasks finished -- %u total real context switches so far\n",
            task_switch_count());
    kprintf("kheap after the stress test:\n");
    kheap_dump();

    kprintf("\nStarting a real bounded-buffer producer/consumer demo: 2 producers, 2 consumers, "
            "a %u-slot shared buffer, %u items each...\n",
            BUFFER_CAPACITY, ITEMS_PER_PRODUCER);
    spinlock_init(&buffer_lock);
    semaphore_init(&sem_empty_slots, (int) BUFFER_CAPACITY);
    semaphore_init(&sem_full_slots, 0);

    int producer_a_id = task_create(producer_a_entry);
    int producer_b_id = task_create(producer_b_entry);
    int consumer_a_id = task_create(consumer_a_entry);
    int consumer_b_id = task_create(consumer_b_entry);
    kprintf("task_create() returned id %d/%d for Producer A/B, id %d/%d for Consumer A/B\n",
            producer_a_id, producer_b_id, consumer_a_id, consumer_b_id);

    while (!task_is_done(producer_a_id) || !task_is_done(producer_b_id) ||
           !task_is_done(consumer_a_id) || !task_is_done(consumer_b_id)) {
        __asm__ volatile ("hlt");
    }

    kprintf("All producer/consumer tasks finished -- %u total real context switches so far\n",
            task_switch_count());

    extern char process_template_normal_start[];
    extern char process_template_normal_end[];
    extern char process_template_reckless_start[];
    extern char process_template_reckless_end[];

    uint32_t normal_template_size = (uint32_t) (uintptr_t) process_template_normal_end -
                                     (uint32_t) (uintptr_t) process_template_normal_start;
    uint32_t reckless_template_size = (uint32_t) (uintptr_t) process_template_reckless_end -
                                       (uint32_t) (uintptr_t) process_template_reckless_start;

    kprintf("\nStarting two real PROCESSES (Process A, Process B), each with its own PRIVATE "
            "page directory -- both run the identical template function, at the identical "
            "fixed virtual address 0x%x, each backed by a DIFFERENT real physical frame...\n",
            PROCESS_CODE_VADDR);

    uint32_t switches_before_processes = task_switch_count();

    /* task_create_process() (017_task.c) builds each process's own
     * private page directory, copies this same template's own compiled
     * machine code into a brand-new physical frame per process, and
     * maps that copy at PROCESS_CODE_VADDR in that process's own
     * directory alone. */
    int process_a_id = task_create_process(process_template_normal_start, normal_template_size);
    int process_b_id = task_create_process(process_template_normal_start, normal_template_size);
    kprintf("task_create_process() returned id %d for Process A, id %d for Process B\n",
            process_a_id, process_b_id);

    /* This chapter's own real ring-0 proof, before either process ever
     * actually runs: walk each process's own page directory by hand,
     * read-only, with paging_translate_in(), and show that the SAME
     * virtual address PROCESS_CODE_VADDR resolves to two DIFFERENT
     * real physical frames -- the whole point of giving each process
     * its own directory in the first place. */
    uint32_t process_a_dir = task_page_directory_phys(process_a_id);
    uint32_t process_b_dir = task_page_directory_phys(process_b_id);
    uint32_t process_a_code_phys = paging_translate_in(process_a_dir, PROCESS_CODE_VADDR);
    uint32_t process_b_code_phys = paging_translate_in(process_b_dir, PROCESS_CODE_VADDR);
    kprintf("Virtual address 0x%x resolves to physical 0x%x in Process A's own directory, "
            "physical 0x%x in Process B's own directory (different frames? %s)\n",
            PROCESS_CODE_VADDR, process_a_code_phys, process_b_code_phys,
            (process_a_code_phys != process_b_code_phys) ? "yes" : "no");

    while (!task_is_done(process_a_id) || !task_is_done(process_b_id)) {
        __asm__ volatile ("hlt");
    }

    uint32_t switches_during_processes = task_switch_count() - switches_before_processes;

    /* A real, independently-checkable LOWER bound on this phase's own
     * switch count, the same honest-by-default style Chapter 16 used
     * for its own ring-3 demo: each of the two processes above calls
     * SYS_YIELD exactly PROCESS_TASK_ITERATIONS times, and every single
     * one of those calls is guaranteed to cause a REAL switch (task 0
     * and the OTHER process are always READY whenever either one
     * yields, so task_yield()'s own round-robin scan never finds
     * `next == old`) -- 2 * PROCESS_TASK_ITERATIONS real switches from
     * SYS_YIELD alone. Each process's own final SYS_EXIT adds exactly
     * one more real switch apiece (task_exit() -> task_yield(), same
     * guarantee) -- 2 more. Any real switches beyond this
     * 2*PROCESS_TASK_ITERATIONS + 2 minimum are real IRQ0 ticks that
     * happened to land somewhere in this narrow window -- purely a
     * fact about real wall-clock timing in this exact run, not
     * something this formula could ever predict in advance. */
    uint32_t expected_minimum_switches = 2u * PROCESS_TASK_ITERATIONS + 2u;
    kprintf("Both processes finished -- %u real context switches during this phase (expected "
            "minimum from SYS_YIELD/SYS_EXIT alone: %u; any excess is real IRQ0 tick "
            "preemption), %u total real context switches since boot\n",
            switches_during_processes, expected_minimum_switches, task_switch_count());

    kprintf("\nStarting a third real process, Process Reckless -- runs one normal cycle, then "
            "deliberately reads virtual address 0x%x, mapped in NO process's own directory...\n",
            UNMAPPED_PROBE_VADDR);

    int reckless_id = task_create_process(process_template_reckless_start, reckless_template_size);
    kprintf("task_create_process() returned id %d for Process Reckless\n", reckless_id);

    /* Expected, and documented here rather than swept under the rug:
     * this wait loop never returns. Process Reckless's own deliberate
     * read faults for real; 017_isr_handlers.c's own page-fault handler
     * (unchanged since Chapter 15) reports it and then halts the ENTIRE
     * machine with `cli; for (;;) hlt;` -- not just Process Reckless's
     * own task -- so this is this chapter's own real run's last line of
     * output, exactly the same honest ending Chapter 15's own one-off
     * ring-3 demo had. */
    while (!task_is_done(reckless_id)) {
        __asm__ volatile ("hlt");
    }

    kprintf("Process Reckless finished (unreachable in this chapter's own real run)\n");
}
