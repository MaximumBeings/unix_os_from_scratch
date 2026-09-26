/* Chapter 15 (its own basic ring-3 plumbing unchanged below) gave this
 * kernel its first real privilege boundary: a Task State Segment
 * (016_tss.h/016_tss.c), ring-3 code/data segments in the GDT
 * (016_gdt.c), a dedicated page-aligned .usermode_text linker section
 * (016_linker.ld, 016_paging.c's PAGE_USER), a real ring 0 -> 3
 * transition (016_usermode.asm), and this book's first real system
 * call (016_isr128.asm, 016_syscall.h, INT 0x80) -- but only as a
 * one-off demo, wired up by hand directly inside kmain(), completely
 * separate from Chapter 11's own scheduler.
 *
 * This chapter makes ring-3 code a real, first-class participant of
 * THAT scheduler instead: 016_task.c's new task_create_usermode()
 * builds each ring-3 task its own dedicated kernel stack (doubling as
 * its own TSS.ESP0 target -- see task_yield()'s own comment on this)
 * and its own real user-mode stack, and starts it through
 * enter_usermode() from task_start_trampoline() exactly like every
 * ring-0 task already starts through a direct call. Two new syscalls
 * (016_syscall.h's SYS_YIELD/SYS_EXIT) are thin ring-0 wrappers around
 * task_yield()/task_exit(), letting a ring-3 task drive its own
 * scheduling decisions through the one sanctioned door back into ring
 * 0 that already existed. This chapter's own demo below creates two
 * real ring-3 tasks that cooperatively SYS_YIELD to each other several
 * times before a real SYS_EXIT -- on top of whatever real IRQ0-driven
 * preemption also happens to land during their run, exactly like
 * Chapter 12's own ring-0 tasks. */

#include <stdint.h>

#include "016_gdt.h"
#include "016_idt.h"
#include "016_keyboard.h"
#include "016_kheap.h"
#include "016_multiboot.h"
#include "016_paging.h"
#include "016_pic.h"
#include "016_pit.h"
#include "016_pmm.h"
#include "016_printf.h"
#include "016_semaphore.h"
#include "016_serial.h"
#include "016_spinlock.h"
#include "016_syscall.h"
#include "016_task.h"
#include "016_vga.h"

#define TIMER_FREQUENCY_HZ 100

/* Deliberately far outside the 0-64 MiB identity-mapped range (which
 * only ever occupies page-directory entries 0-15): 0xC0000000 >> 22 =
 * 768. Mapping here forces paging_map_page() to allocate a genuinely
 * new page table rather than reusing one paging_init() already built. */
#define TEST_VIRT_ADDR 0xC0000000u

/* Defined by 016_linker.ld, not by this file -- the linker is the one
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

/* How many times each of this chapter's two ring-3 tasks prints and
 * then SYS_YIELDs before its own real SYS_EXIT. Deliberately small: a
 * real, independently-checkable lower bound on this chapter's own
 * final switch count falls straight out of this one constant (see
 * kmain()'s own comment on it below), the same way Chapter 12's own
 * TIMER_FREQUENCY_HZ made that chapter's tick count independently
 * checkable. */
#define RING3_TASK_ITERATIONS 5u

/* This chapter's shared ring-3 syscall wrapper -- both demo tasks
 * below call this instead of repeating the same inline-asm INT 0x80
 * sequence Chapter 15's own user_mode_entry() wrote out by hand. Genuinely
 * runs at CPL 3 (marked into 016_linker.ld's own dedicated
 * .usermode_text section, exactly like every other function in this
 * file that needs to), so it cannot call kprintf() directly -- that
 * would try to touch the serial/VGA drivers' own privileged I/O ports
 * (forbidden at CPL 3, see 016_tss.c's own iomap_base comment) -- this
 * IS the only sanctioned door back into ring 0. `num` and `arg` are
 * this book's own real i386 syscall convention (OSDev Wiki, "System
 * Calls": Linux's own real convention "gets its arguments in eax,
 * ebx, ecx, edx, esi, edi, and ebp in that order" -- this book only
 * ever needs the first two), loaded into explicit register variables
 * right before the real INT 0x80 instruction; nothing about this is a
 * normal C function call underneath. */
__attribute__((section(".usermode_text")))
static void usermode_syscall(uint32_t num, uint32_t arg) {
    register uint32_t sys_num asm("eax") = num;
    register uint32_t sys_arg asm("ebx") = arg;
    __asm__ volatile ("int $0x80" :: "r" (sys_num), "r" (sys_arg) : "memory");
}

/* This chapter's two real ring-3 demo tasks -- the entire reason
 * 016_task.c's new task_create_usermode() exists. Each one prints its
 * own static message (SYS_WRITE_STR), then genuinely gives up the CPU
 * with a real SYS_YIELD -- a real task_yield() call, reached through
 * ring 0, picking the next READY task in this kernel's one shared
 * round-robin scan exactly like any ring-0 task's own task_yield()
 * call would -- RING3_TASK_ITERATIONS times, before a final real
 * SYS_EXIT. Neither message string is itself marked PAGE_USER, and
 * neither task ever needs to be: forming a pointer to a string literal
 * is a compile-time constant, never a memory access, and the only code
 * that ever actually DEREFERENCES that pointer is isr128_handler()
 * (016_isr_handlers.c), which runs at ring 0 and can read any PRESENT
 * page regardless of the U/S bit -- exactly the same reasoning
 * Chapter 15's own user_mode_entry() already relied on. */
__attribute__((section(".usermode_text")))
static void ring3_task_a_entry(void) {
    const char *msg = "  Ring3 Task A: printed via a real SYS_WRITE_STR, "
                       "now yielding via a real SYS_YIELD\n";
    for (uint32_t i = 1; i <= RING3_TASK_ITERATIONS; i++) {
        usermode_syscall(SYS_WRITE_STR, (uint32_t) (uintptr_t) msg);
        usermode_syscall(SYS_YIELD, 0u);
    }
    usermode_syscall(SYS_EXIT, 0u);

    /* Unreachable in this chapter's own real run: SYS_EXIT's own
     * isr128_handler() case calls task_exit() directly, which never
     * returns (016_task.c) -- this task's entire kernel stack,
     * including this exact call frame, is simply abandoned at that
     * point. HLT would itself be a second privileged instruction this
     * task has no right to execute at CPL 3 (exactly like Chapter 15's
     * own deliberate CLI demo), so this defensive tail is a plain
     * empty spin, never CLI or HLT. */
    for (;;) { }
}

__attribute__((section(".usermode_text")))
static void ring3_task_b_entry(void) {
    const char *msg = "  Ring3 Task B: printed via a real SYS_WRITE_STR, "
                       "now yielding via a real SYS_YIELD\n";
    for (uint32_t i = 1; i <= RING3_TASK_ITERATIONS; i++) {
        usermode_syscall(SYS_WRITE_STR, (uint32_t) (uintptr_t) msg);
        usermode_syscall(SYS_YIELD, 0u);
    }
    usermode_syscall(SYS_EXIT, 0u);

    for (;;) { }
}

void kmain(uint32_t magic, uint32_t mboot_info_addr) {
    serial_init();
    vga_init();
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);

    kprintf("Unix OS from Scratch -- Chapter 16: kernel entry reached\n");

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

    kprintf("\nStarting two real RING-3 tasks in this kernel's own scheduler (Ring3 Task A, "
            "Ring3 Task B) -- each one runs at CPL 3, and drives its own scheduling with real "
            "INT 0x80 SYS_YIELD/SYS_EXIT syscalls, on top of whatever real IRQ0-driven "
            "preemption also happens to land during their run...\n");

    uint32_t switches_before_ring3 = task_switch_count();

    /* task_create_usermode() (016_task.c) does everything Chapter 15
     * did by hand directly inside this function -- a dedicated kernel
     * stack doubling as this task's own TSS.ESP0 target, a real
     * PAGE_USER user stack, and (the first time only) marking
     * 016_linker.ld's own .usermode_text pages user-accessible -- and
     * hands back an ordinary task index, exactly like task_create()
     * already does for ring-0 tasks. */
    int ring3_a_id = task_create_usermode(ring3_task_a_entry);
    int ring3_b_id = task_create_usermode(ring3_task_b_entry);
    kprintf("task_create_usermode() returned id %d for Ring3 Task A, id %d for Ring3 Task B\n",
            ring3_a_id, ring3_b_id);

    while (!task_is_done(ring3_a_id) || !task_is_done(ring3_b_id)) {
        __asm__ volatile ("hlt");
    }

    uint32_t switches_during_ring3 = task_switch_count() - switches_before_ring3;

    /* A real, independently-checkable LOWER bound on this phase's own
     * switch count, the same honest-by-default style Chapter 12 used
     * for its own tick-driven demo: each of the two tasks above calls
     * SYS_YIELD exactly RING3_TASK_ITERATIONS times, and every single
     * one of those calls is guaranteed to cause a REAL switch (task 0
     * and the OTHER ring-3 task are always READY whenever either one
     * yields, so task_yield()'s own round-robin scan never finds
     * `next == old`) -- 2 * RING3_TASK_ITERATIONS real switches from
     * SYS_YIELD alone. Each task's own final SYS_EXIT adds exactly one
     * more real switch apiece (task_exit() -> task_yield(), same
     * guarantee) -- 2 more. Any real switches beyond this
     * 2*RING3_TASK_ITERATIONS + 2 minimum are real IRQ0 ticks that
     * happened to land somewhere in this narrow window -- purely a
     * fact about real wall-clock timing in this exact run, not
     * something this formula could ever predict in advance. */
    uint32_t expected_minimum_switches = 2u * RING3_TASK_ITERATIONS + 2u;
    kprintf("Both ring-3 tasks finished -- %u real context switches during this phase "
            "(expected minimum from SYS_YIELD/SYS_EXIT alone: %u; any excess is real "
            "IRQ0 tick preemption), %u total real context switches since boot\n",
            switches_during_ring3, expected_minimum_switches, task_switch_count());
}
