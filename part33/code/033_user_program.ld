/* This chapter's second, genuinely independent linker script -- it has
 * nothing to do with 033_linker.ld above, which places this KERNEL
 * image at the physical 1 MiB mark GRUB itself loads it at. This one
 * places 033_user_program.c's own compiled code at a fixed VIRTUAL
 * address this program will only ever actually run at once 033_elf.c's
 * own elf_load() has mapped it there, deliberately chosen well clear of
 * every fixed address 033_task.h already uses (PROCESS_CODE_VADDR
 * 0xE8000000u, PROCESS_STACK_VADDR 0xE8001000u) so this chapter's own
 * real captured evidence can only be explained by this loader reading
 * this program's own real p_vaddr fields, never by coincidentally
 * reusing a number this kernel's own source already had lying around. */
ENTRY(_start)
SECTIONS
{
    . = 0xE9000000;
    .text   : { *(.text) }
    .rodata : { *(.rodata) }
    .data   : { *(.data) }
    .bss    : { *(.bss) }
}
