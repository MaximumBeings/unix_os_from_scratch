/* This file is this chapter's own real proof: it is NOT part of this
 * kernel image at all. It is compiled with its own separate `gcc -m32
 * -ffreestanding -c` invocation, linked with its own separate `ld -m
 * elf_i386 -T 032_user_program.ld` invocation and its own dedicated
 * linker script -- fixing this program's own load address at 0xE9000000,
 * deliberately a different number than 032_task.h's own
 * PROCESS_CODE_VADDR (0xE8000000u), so that a real virtual address
 * showing up at runtime at 0xE9000000 could only ever have come from
 * THIS FILE's own linker script, never from anything this kernel's own
 * source chose on its behalf -- and the result is embedded, whole, as a
 * real GRUB Multiboot2 boot MODULE (032_multiboot.h's own struct
 * multiboot_tag_module) alongside kernel.bin, not linked into it.
 * 032_elf.c's own elf_load() reads this file's real ELF header and
 * program headers at boot time and maps its segments from those real
 * numbers alone -- this program's own entry point, wherever it ends up
 * in this kernel's `readelf -h` output, is what 032_kmain.c's own
 * kmain() actually runs, not a constant kmain() ever had to know in
 * advance.
 *
 * `_start` is this program's own real ELF entry point -- exactly the
 * symbol 032_user_program.ld's own ENTRY(_start) directive names, and
 * exactly the symbol whose real, linked address ends up in this file's
 * own e_entry field once `ld` is done. Its body is otherwise identical
 * in style to Chapter 17's own process_template_normal(): print via a
 * real SYS_WRITE_STR, yield via a real SYS_YIELD, USER_PROGRAM_
 * ITERATIONS times, then a real SYS_EXIT -- every syscall an inlined
 * `int $0x80` sequence rather than a call to any shared helper
 * function, for the same reason Chapter 17's own templates used inlined
 * syscalls: this file's own internal jumps and branches stay correct
 * wherever the linker actually places them (x86 near jumps are
 * PC-relative), but this program links and loads as one single,
 * complete, self-contained unit to begin with -- there is no second
 * "copied separately" piece of code the way Chapter 17's own template-
 * copying task_create_process() needed to worry about, so the inlined
 * style here is simply this file's own natural, ordinary form, not a
 * workaround. */
#include <stdint.h>

#include "032_syscall.h"
#include "032_user_program.h"

void _start(void) {
    const char *msg = "  printed via a real SYS_WRITE_STR, now yielding via a real SYS_YIELD "
                       "(this line runs from a genuinely separate, separately linked ELF file)\n";

    for (uint32_t i = 1; i <= USER_PROGRAM_ITERATIONS; i++) {
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

    /* Unreachable in this chapter's own real run, for exactly the same
     * reason Chapter 17's own templates documented here: SYS_EXIT's own
     * isr128_handler() case calls task_exit() directly and never
     * returns. A plain empty spin, never CLI or HLT -- both privileged
     * instructions this program has no right to execute at CPL 3. */
    for (;;) { }
}
