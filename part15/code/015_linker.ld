/* Chapter 7 added kernel_end, the linker's own real answer to "where
 * does the kernel image actually stop in physical memory." This
 * chapter adds a second linker-defined range for the same reason:
 * this chapter's own ring-3 demo function has to be marked
 * PAGE_USER (015_paging.h) so the CPU will let CPL 3 fetch
 * instructions from it at all -- but PAGE_USER is granted per 4 KiB
 * page, and C gives no promise that a single function's machine code
 * starts and ends on a page boundary, or even that it does not share
 * a page with some ordinary ring-0-only kernel code sitting right
 * next to it in .text. A dedicated, page-aligned .usermode_text
 * section, placed between .text and .rodata and marked with its own
 * real symbols, is what lets 015_kmain.c mark EXACTLY the pages this
 * one demo function occupies as user-accessible -- and nothing else
 * in the whole kernel image. */
ENTRY(_start)
SECTIONS
{
    . = 1M;
    .multiboot_header : { *(.multiboot_header) }
    .text   : { *(.text) }

    . = ALIGN(4096);
    usermode_text_start = .;
    .usermode_text : { *(.usermode_text) }
    . = ALIGN(4096);
    usermode_text_end = .;

    .rodata : { *(.rodata) }
    .data   : { *(.data) }
    .bss    : { *(.bss) }
    kernel_end = .;
}
