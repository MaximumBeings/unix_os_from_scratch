/* Chapter 7 added kernel_end, the linker's own real answer to "where
 * does the kernel image actually stop in physical memory." Chapter
 * 16 added a second linker-defined range for the same underlying
 * reason -- PAGE_USER (017_paging.h) is granted per 4 KiB page, and C
 * gives no promise that a function's machine code starts and ends on
 * a page boundary -- but this chapter's own design (every process
 * gets its own PRIVATE address space, see 017_paging.c's
 * paging_new_address_space()) no longer runs any ring-3 code directly
 * out of the kernel image at all: task_create_process()
 * (017_task.c) instead physically COPIES one whole template function
 * into a brand-new physical frame per process, and maps only THAT
 * copy as PAGE_USER. What this linker script owes each template is
 * not "mark these kernel-image pages user-accessible" but "give me a
 * clean, page-aligned, exactly-one-page range to copy FROM" -- one
 * dedicated section per template, so 017_kmain.c's own
 * process_template_normal() and process_template_reckless() can never
 * accidentally share a page with each other or with ordinary .text,
 * and task_create_process() can compute exactly how many bytes to
 * copy from each one's own real linker-defined start/end symbols. */
ENTRY(_start)
SECTIONS
{
    . = 1M;
    .multiboot_header : { *(.multiboot_header) }
    .text   : { *(.text) }

    . = ALIGN(4096);
    process_template_normal_start = .;
    .process_template_normal : { *(.process_template_normal) }
    . = ALIGN(4096);
    process_template_normal_end = .;

    . = ALIGN(4096);
    process_template_reckless_start = .;
    .process_template_reckless : { *(.process_template_reckless) }
    . = ALIGN(4096);
    process_template_reckless_end = .;

    .rodata : { *(.rodata) }
    .data   : { *(.data) }
    .bss    : { *(.bss) }
    kernel_end = .;
}
