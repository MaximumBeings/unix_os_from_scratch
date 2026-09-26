/* Chapter 7: unchanged in shape from every prior chapter, still loading
 * at the conventional Multiboot2 1 MiB physical mark -- with exactly one
 * new symbol, kernel_end, defined as whatever address the linker itself
 * lands on right after the last real section. This chapter's own
 * physical memory manager needs to know where the kernel image actually
 * stops, in physical memory, so it never hands out a frame this kernel
 * is still standing on -- and the linker is the one piece of this
 * toolchain that genuinely knows that address, not a guess baked into
 * the C source. */
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
