/* Chapter 5: places the kernel at the 1 MiB physical-address mark, the
 * conventional Multiboot2 load address (below it is real-mode-era memory
 * GRUB and the BIOS still use; above it is free for us) -- and lists the
 * multiboot header section FIRST so it lands within the first 8 KiB of
 * the file, where the Multiboot2 spec requires GRUB to be able to find it. */
ENTRY(_start)
SECTIONS
{
    . = 1M;
    .multiboot_header : { *(.multiboot_header) }
    .text   : { *(.text) }
    .rodata : { *(.rodata) }
    .data   : { *(.data) }
    .bss    : { *(.bss) }
}
