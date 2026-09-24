/* Chapters 1-14 (all unchanged below) built a kernel that runs
 * entirely at ring 0 -- every task, every driver, every line of C
 * this book has written so far has run with full hardware privilege,
 * trusted completely. A real OS cannot stay that way once it runs
 * code it did not write itself: this chapter gives this kernel its
 * first real privilege boundary. It builds a Task State Segment
 * (015_tss.h/015_tss.c) purely so the CPU has somewhere to find a
 * real kernel stack the instant ring-3 code raises an interrupt; adds
 * ring-3 code/data segments to the GDT (015_gdt.c); marks this
 * chapter's own small demo function -- and only that function, via a
 * dedicated linker section -- as executable from ring 3
 * (015_linker.ld, 015_paging.c's new PAGE_USER); and performs a real
 * privilege-level transition into it (015_usermode.asm). That ring-3
 * code cannot call kprintf() directly -- it has no way to reach the
 * VGA/serial drivers' own privileged I/O ports -- so this chapter
 * also adds this book's first real system call (015_isr128.asm,
 * 015_syscall.h, INT 0x80), the one sanctioned door back into ring 0.
 * Finally, ring-3 code deliberately executes CLI, a privileged
 * instruction, to prove -- with a real captured #GP fault, not just
 * an assertion -- that the CPU is actually enforcing all of this. */

#include <stdint.h>

#include "015_gdt.h"
#include "015_idt.h"
#include "015_keyboard.h"
#include "015_kheap.h"
#include "015_multiboot.h"
#include "015_paging.h"
#include "015_pic.h"
#include "015_pit.h"
#include "015_pmm.h"
#include "015_printf.h"
#include "015_semaphore.h"
#include "015_serial.h"
#include "015_spinlock.h"
#include "015_syscall.h"
#include "015_task.h"
#include "015_tss.h"
#include "015_vga.h"

#define TIMER_FREQUENCY_HZ 100

/* Deliberately far outside the 0-64 MiB identity-mapped range (which
 * only ever occupies page-directory entries 0-15): 0xC0000000 >> 22 =
 * 768. Mapping here forces paging_map_page() to allocate a genuinely
 * new page table rather than reusing one paging_init() already built. */
#define TEST_VIRT_ADDR 0xC0000000u

/* Size of the dedicated kernel stack this chapter kmalloc()s for
 * TSS.ESP0 -- the same 4 KiB every ordinary task stack in this book
 * has used since Chapter 11 (015_task.c's own TASK_STACK_SIZE), for
 * the same reason: plenty of room for a real ISR stub plus one C
 * handler's own local variables, several times over. */
#define TASK_STACK_SIZE_FOR_RING3 4096u

/* Defined by 015_linker.ld, not by this file -- the linker is the one
 * part of this toolchain that genuinely knows where the kernel image's
 * last real section ends in physical memory. */
extern char kernel_end[];

/* Also defined by 015_linker.ld: the real, page-aligned bounds of the
 * dedicated .usermode_text section this chapter's own user_mode_entry()
 * (below) is placed into, via its own section attribute. */
extern char usermode_text_start[];
extern char usermode_text_end[];

/* 015_usermode.asm's real ring 0 -> ring 3 transition. Never returns
 * in the ordinary sense -- the task it starts keeps running at ring 3
 * (and, in this chapter's own demo, ends by deliberately faulting)
 * independently of whatever called it. */
extern void enter_usermode(void (*entry)(void), void *user_stack_top);

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

/* This chapter's own ring-3 demo -- the entire reason 015_linker.ld
 * gives it a dedicated page-aligned section. Genuinely runs at CPL 3:
 * it cannot call kprintf() directly (that would try to touch the
 * serial/VGA drivers' own I/O ports, forbidden at CPL 3 -- see
 * 015_tss.c's own iomap_base comment), so the only way it can print
 * anything at all is this chapter's own real syscall. */
__attribute__((section(".usermode_text")))
static void user_mode_entry(void) {
    const char *msg =
        "  Ring 3 says hello -- this line printed via a real INT 0x80 "
        "syscall, not a direct kprintf() call.\n";

    /* OSDev Wiki, "System Calls": Linux's own i386 convention "gets
     * its arguments in eax, ebx, ecx, edx, esi, edi, and ebp in that
     * order" -- this book only ever needs the first two. Explicit
     * register variables put SYS_WRITE_STR and `msg` directly into
     * EAX/EBX right before the real INT 0x80 instruction; nothing
     * about this is a normal C function call. */
    register uint32_t sys_num asm("eax") = SYS_WRITE_STR;
    register uint32_t sys_arg asm("ebx") = (uint32_t) (uintptr_t) msg;
    __asm__ volatile ("int $0x80" :: "r" (sys_num), "r" (sys_arg));

    /* This chapter's real proof that CPL is genuinely 3 here, not
     * merely claimed to be: CLI is a privileged instruction (OSDev
     * Wiki, "Exceptions", vector 13 / #GP: one listed trigger is
     * "Executing a privileged instruction while CPL != 0"). If the
     * transition into ring 3 were somehow silently a no-op, this line
     * would succeed and prove nothing at all; instead, this book's
     * own real captured run shows the CPU raising a genuine #GP right
     * here, caught by 015_isr_handlers.c's own isr13_handler(). */
    __asm__ volatile ("cli");

    /* Unreachable in this chapter's own real run: isr13_handler()
     * never resumes the faulting instruction (same policy as every
     * other exception handler in this book since Chapter 10), so
     * control never returns here. */
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

void kmain(uint32_t magic, uint32_t mboot_info_addr) {
    serial_init();
    vga_init();
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);

    kprintf("Unix OS from Scratch -- Chapter 15: kernel entry reached\n");

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

    kprintf("\nSetting up ring 3: a dedicated kernel stack for the TSS, a real user "
            "stack, and marking this chapter's own demo code user-accessible...\n");

    /* A real, dedicated kernel stack for TSS.ESP0 -- the exact stack
     * the CPU switches to the instant ring-3 code raises anything at
     * all. This kernel is not currently doing anything else with the
     * heap, so a plain kmalloc() is enough; a later chapter giving
     * each task its own kernel stack would call tss_set_kernel_stack()
     * again on every switch, the same way this book's own TCB already
     * tracks each task's ordinary stack. */
    void *ring3_kernel_stack = kmalloc(TASK_STACK_SIZE_FOR_RING3);
    tss_set_kernel_stack((uint32_t) (uintptr_t) ring3_kernel_stack + TASK_STACK_SIZE_FOR_RING3);

    /* A real user stack, one frame straight from Chapter 7's own
     * pmm_alloc_frame() -- identity-mapped, so its physical address is
     * also a valid virtual address, exactly like every other frame
     * this kernel has ever mapped. PAGE_USER is what makes it usable
     * from CPL 3 at all; without it, the very first PUSH this task's
     * own prologue performs would #PF instantly. */
    uint32_t user_stack_frame = pmm_alloc_frame();
    paging_map_page(user_stack_frame, user_stack_frame, PAGE_PRESENT | PAGE_RW | PAGE_USER);
    uint32_t user_stack_top = user_stack_frame + 4096u;

    /* Marks EXACTLY the pages 015_linker.ld's own page-aligned
     * .usermode_text section occupies as user-accessible -- no RW bit
     * here at all, since this range only ever needs to be FETCHED
     * from, never written to. Every other page in this entire kernel
     * image, including every driver and every other function in this
     * very file, stays exactly as ring-0-only as it has been since
     * Chapter 8 first turned paging on. */
    uint32_t text_start = (uint32_t) (uintptr_t) usermode_text_start;
    uint32_t text_end   = (uint32_t) (uintptr_t) usermode_text_end;
    for (uint32_t addr = text_start; addr < text_end; addr += 4096u) {
        paging_map_page(addr, addr, PAGE_PRESENT | PAGE_USER);
    }
    kprintf("Marked %u page(s) [0x%x - 0x%x) of .usermode_text user-accessible\n",
            (text_end - text_start) / 4096u, text_start, text_end);

    kprintf("\nEntering ring 3 for the first time...\n");
    enter_usermode(user_mode_entry, (void *) (uintptr_t) user_stack_top);

    /* Unreachable in this chapter's own real run: user_mode_entry()
     * always ends by deliberately triggering a #GP, and
     * isr13_handler() halts rather than ever returning here. */
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
