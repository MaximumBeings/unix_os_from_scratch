/* Chapter 7 added kernel_end, the linker's own real answer to "where
 * does the kernel image actually stop in physical memory." Chapter 17
 * added two dedicated page-aligned sections here so its own kmain()
 * could copy baked-in template functions out of the kernel image, one
 * per process. This chapter's own demo needs neither: its two demo
 * processes run a REAL, separately compiled ELF executable, loaded
 * from a real GRUB boot module (034_multiboot.h's own
 * multiboot_tag_module) rather than from anything living inside this
 * kernel image at all -- so this script goes back to exactly what
 * Chapters 7-15 already needed, one kernel_end symbol and nothing
 * more. 034_task.c's own task_create_process() (Chapter 17's own
 * template-copying function) is still real, carried-forward
 * infrastructure -- it simply has no caller left in this chapter's own
 * kmain(), which is why the two dedicated sections it depended on are
 * the one real difference between this file and Chapter 17's own. */
ENTRY(_start)
SECTIONS
{
    . = 1M;
    .multiboot_header : { *(.multiboot_header) }
    .text   : { *(.text) }
    .rodata : { *(.rodata) }
    .data   : { *(.data) }
    .bss    : { *(.bss) }
    kernel_end = .;
}
