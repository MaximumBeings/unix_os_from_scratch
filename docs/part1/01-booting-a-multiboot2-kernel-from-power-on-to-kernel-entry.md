# 1. Booting a Multiboot2 Kernel: From Power-On to Kernel Entry

**What you will understand:** what a real x86-64 machine actually does between the moment you press power and the moment any code you wrote begins running; why this book does not write its own bootloader, and what GRUB and the Multiboot2 standard do instead; the exact, real, spec-required shape of a Multiboot2 header (checked against GNU's own Multiboot2 Specification, quoted directly below rather than paraphrased from memory); and why a kernel's very first C function has to talk to raw hardware ports before it can print a single character, since there is no operating system underneath it to ask.

**What you need to know first:** working C (functions, pointers, the `static` keyword) and enough x86 assembly to read a short, heavily-commented file (you do not need to write assembly from scratch -- every instruction used here is explained where it appears). No prior OS-development experience is assumed.

## What "booting" actually means

Every program you have ever run -- in this book's sibling series or anywhere else -- ran *on top of* an operating system. `malloc` asked the OS for memory. `printf` asked the OS to write to a file descriptor. Even a CUDA kernel launch went through an OS-owned driver. This book builds the thing all of that sits on, and that means starting from a moment none of those sibling books ever had to think about: the instant the CPU has power and *nothing else exists yet*. No memory allocator, no filesystem, no notion of "a process," not even a guarantee that the .bss segment has been zeroed by anyone but us.

Getting from that moment to a running kernel is a real, well-defined chain of handoffs, not a single magic step:

```text
+-------------------------------------------------------------------+
|  From power-on to your first line of C, one real step at a time    |
|                                                                     |
|  [1] BIOS / UEFI firmware                                          |
|      - runs first, built into the motherboard                      |
|      - finds a bootable device, loads ITS first stage               |
|      |                                                              |
|  [2] GRUB (the bootloader we did NOT write)                         |
|      - reads /boot/grub/grub.cfg from the ISO                       |
|      - finds kernel.bin, scans its first 32KB for a real            |
|        Multiboot2 header (magic 0xE85250D6)                         |
|      - loads kernel.bin into memory at 1 MiB, switches the CPU      |
|        into 32-bit protected mode, and jumps to its entry point     |
|      |                                                              |
|  [3] Our _start (001_boot.asm)                                      |
|      - CPU is already in 32-bit protected mode (GRUB's job, not     |
|        ours -- real-mode setup and the mode switch are GRUB's own   |
|        real, established work this book does not redo)              |
|      - no stack yet -- first instruction sets esp                   |
|      |                                                              |
|  [4] Our kmain() (001_kmain.c)                                      |
|      - the first C function THIS BOOK wrote that the machine runs   |
|      - talks to real hardware (the COM1 UART) directly, no OS       |
|        underneath to ask for help                                   |
+-------------------------------------------------------------------+
```

Steps [1] and [2] are real, existing software this book deliberately does not reimplement -- writing a BIOS/UEFI-compatible bootloader from scratch is its own, mostly unrelated project (real-mode 16-bit assembly, A20 line handling, a hand-rolled protected-mode switch), and it would teach almost nothing about what an *operating system* actually does once it is running. GRUB already solves that problem correctly, for real hardware, today. What this book owns starts at step [3]: the moment GRUB hands control to code we wrote, in a well-defined machine state that a real, published standard -- Multiboot2 -- guarantees.

## The Multiboot2 header: the one contract GRUB requires

GRUB does not know or care what your kernel does. It cares about exactly one thing before it will load and jump to a binary: does that binary contain a correctly-formed Multiboot2 header, somewhere in its first 32 KiB? GNU's own Multiboot2 Specification states the required fields plainly, quoted here rather than paraphrased:

> "The field 'magic' is the magic number identifying the header, which must be the hexadecimal value 0xE85250D6."

> "The field 'header_length' specifies the Length of Multiboot2 header in bytes including magic fields."

> "The field 'checksum' is a 32-bit unsigned value which, when added to the other magic fields (i.e. 'magic', 'architecture' and 'header_length'), must have a 32-bit unsigned sum of zero."

> "Tags are terminated by a tag of type '0' and size '8'."

(GNU Multiboot2 Specification, via the GNU GRUB manual: https://www.gnu.org/software/grub/manual/multiboot2/multiboot.html)

Every one of those four requirements maps directly onto a field in `001_boot.asm` below, in the same order:

```text
+------------------------------------------------------------+
|  001_boot.asm's own Multiboot2 header, field by field        |
|                                                                |
|  offset 0   magic          0xE85250D6   (fixed, spec-defined) |
|  offset 4   architecture   0            (0 = i386 protected)  |
|  offset 8   header_length  header_end - header_start (bytes)  |
|  offset 12  checksum       0x100000000 - (magic+arch+len)     |
|                            -- forces magic+arch+len+checksum  |
|                               to sum to zero mod 2^32, which  |
|                               is exactly what GRUB checks     |
|  offset 16  end tag: type=0, size=8 (flags=0, implied)        |
+------------------------------------------------------------+
```

`0x100000000 - x` is two's-complement subtraction from 2^32: adding it back to `x` always produces exactly `2^32`, which is `0` in 32-bit unsigned arithmetic -- precisely the "32-bit unsigned sum of zero" the spec requires, computed by the assembler once at build time rather than by us doing modular arithmetic by hand.

## `001_boot.asm`: the entry point GRUB actually jumps to

By the time GRUB jumps to `_start`, it has already done real, nontrivial work on our behalf: switched the CPU into 32-bit protected mode, loaded `kernel.bin` at the 1 MiB physical address, and set up a handful of machine-state guarantees the Multiboot2 spec promises (a valid GDT, paging disabled, interrupts disabled). What it has *not* done is give us a stack -- `esp` is whatever GRUB last left it as, not something a C function can safely build a call frame on. `_start`'s only two jobs, in order, are to fix that and then get out of the way:

```nasm
; Chapter 1: the Multiboot2 header GRUB scans for, plus the tiny assembly
; entry point every C function in this book ultimately runs under. GRUB
; hands control to _start in 32-bit protected mode with no stack set up
; and no guarantee the .bss segment has been zeroed by anything but us --
; so this file's only two jobs are: point esp at a real stack, then call
; into C.
BITS 32

section .multiboot_header
align 8
header_start:
    dd 0xe85250d6                ; magic number (multiboot2)
    dd 0                         ; architecture 0 (protected mode i386)
    dd header_end - header_start ; header length
    dd 0x100000000 - (0xe85250d6 + 0 + (header_end - header_start)) ; checksum
    ; end tag
    dw 0
    dw 0
    dd 8
header_end:

section .text
extern kmain
global _start
_start:
    mov esp, stack_top
    call kmain
    cli
.hang:
    hlt
    jmp .hang

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:
```

`stack_bottom`/`stack_top` live in `.bss`, a 16 KiB block the linker script below reserves but the file itself stores no bytes for (that is what `.bss` -- "block started by symbol" -- means: size without file content). `mov esp, stack_top` points the stack pointer at the *high* end of that block, since x86 stacks grow downward; `call kmain` is then a completely ordinary C function call, with a real stack underneath it for the first time. If `kmain` ever returns (this chapter's own `kmain` won't, once the last section below is in place), `cli`/`hlt`/`jmp .hang` disables interrupts and parks the CPU in a halt loop rather than falling off the end of `_start` into undefined memory.

## `001_linker.ld`: placing the kernel where GRUB expects it

```text
/* Chapter 1: places the kernel at the 1 MiB physical-address mark, the
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
```

`. = 1M` places the very first byte of `.multiboot_header` at physical address `0x00100000` -- the conventional Multiboot2 load address, chosen because everything below 1 MiB is real-mode-era memory the BIOS and GRUB itself may still be using. Listing `.multiboot_header` as the *first* section in `SECTIONS` is not cosmetic: the Multiboot2 spec requires GRUB to find the header within the binary's first 32 KiB, and without this ordering nothing stops the linker from placing `.text` first and pushing the header past that boundary. The resulting physical memory layout:

```text
+-------------------------------------------------------+
|  Physical memory after GRUB hands off (001_linker.ld)   |
|                                                           |
|  0x00000000 +----------------------------+               |
|              | BIOS / real-mode-era data  |  (untouched)  |
|  0x00100000  +----------------------------+  (== 1 MiB)   |
|              | .multiboot_header          |  must sit     |
|              |                            |  within the   |
|              |                            |  first 8 KiB  |
|              +----------------------------+               |
|              | .text  (_start, kmain, ...)|               |
|              +----------------------------+               |
|              | .rodata                    |               |
|              +----------------------------+               |
|              | .data                      |               |
|              +----------------------------+               |
|              | .bss   (stack_bottom..top) |  our stack    |
|              +----------------------------+               |
+-------------------------------------------------------+
```

## `001_kmain.c`: the first C this book's machine has ever run

`kmain` has no `printf`, no `stdout`, nothing -- those are OS services, and step [4] in the very first diagram above *is* the OS being built. The only way to produce output right now is to talk to a real hardware device directly, through the x86 `in`/`out` instructions, which read and write a CPU I/O port rather than a memory address. This chapter uses the COM1 serial UART (I/O ports `0x3F8`-`0x3FF`) rather than the VGA text-mode framebuffer for one deliberate reason: QEMU's `-serial stdio` flag turns that UART's real output into plain text on this book's own terminal, which can be captured exactly and locked into this page -- no screenshot of a virtual monitor required, the same "genuinely run, exact output locked in" discipline this book's sibling series has followed throughout.

```c
/* Chapter 1: our first freestanding "kernel". It writes to the COM1 serial
 * port rather than the VGA text buffer so this book can lock its exact,
 * real, captured text output into the page -- no screenshot of a virtual
 * monitor needed, just QEMU's own -serial stdio flag. Everything above
 * main() (the boot stub in 001_boot.asm and this file's own outb/inb pair)
 * exists because, the instant GRUB hands control to _start, there is no
 * C runtime, no stack the compiler can assume, and no OS underneath us to
 * ask for I/O -- kmain() below is the first C function this machine has
 * ever run that WE wrote, and serial_puts() is the first output any part
 * of this book produces without an operating system's help. */

static inline void outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

#define COM1 0x3F8

static void serial_init(void) {
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}

static int serial_transmit_empty(void) {
    return inb(COM1 + 5) & 0x20;
}

static void serial_putc(char c) {
    while (!serial_transmit_empty()) { }
    outb(COM1, (unsigned char)c);
}

static void serial_puts(const char *s) {
    for (; *s; s++) serial_putc(*s);
}

/* QEMU's isa-debug-exit device (real, standard OS-dev technique): a write
 * to I/O port 0xf4 makes QEMU itself exit with status (value << 1) | 1,
 * instead of the kernel having to halt forever and an outside process
 * having to kill QEMU to get its output back. Without real hardware
 * behind this port, this instruction is meaningless outside QEMU -- a
 * real kernel bound for real hardware would never do this; it exists here
 * only so this chapter's own locked output ends cleanly and
 * deterministically, without a stray "killed by timeout" line that has
 * nothing to do with the kernel's own behavior. */
#define QEMU_DEBUG_EXIT_PORT 0xf4

static void qemu_exit(unsigned char code) {
    outb(QEMU_DEBUG_EXIT_PORT, code);
}

void kmain(void) {
    serial_init();
    serial_puts("Unix OS from Scratch -- Chapter 1: kernel entry reached\n");
    serial_puts("multiboot2 + nasm + freestanding gcc + ld + qemu pipeline: OK\n");
    qemu_exit(0);
}
```

`outb`/`inb` are thin wrappers around the x86 `outb`/`inb` instructions (inline assembly, `"a"(val)` and `"Nd"(port)` pin the value into register `al` and the port into `dx`, which is what those instructions require). `serial_init` is the standard 8250/16550 UART initialization sequence -- disable interrupts, set the baud-rate divisor for 38400 baud, select 8 data bits/no parity/1 stop bit, and enable a 14-byte FIFO -- taking this chapter's word for the specific register values here; a later chapter revisits the UART on its own terms rather than re-deriving the full 8250 programming model in a chapter about booting. `serial_transmit_empty` polls the UART's line-status register until it reports the transmit buffer is free, and `serial_putc`/`serial_puts` build ordinary character/string output on top of that one primitive -- this book's very first "print statement," running with no operating system underneath it at all.

The final piece, `qemu_exit`, is not part of a real kernel at all: it writes to I/O port `0xf4`, which QEMU's own `isa-debug-exit` device (enabled below with `-device isa-debug-exit,iobase=0xf4,iosize=0x04`) turns into a clean, deterministic QEMU process exit -- exit code `(value << 1) | 1`. On real hardware this instruction would do nothing meaningful; it exists purely so this chapter's own locked output below ends the moment the kernel is done, rather than needing an external process to kill a kernel that halts forever.

## Building and booting it, for real

```bash
nasm -f elf32 001_boot.asm -o boot.o
gcc -m32 -ffreestanding -fno-stack-protector -fno-pic -Wall -Wextra -c 001_kmain.c -o kmain.o
ld -m elf_i386 -T 001_linker.ld -o kernel.bin boot.o kmain.o
grub-file --is-x86-multiboot2 kernel.bin
```

**Output (cloud sandbox -- real, live-executed build output):**

```text
=== nasm -f elf32 001_boot.asm -o boot.o ===
(no output = clean assemble)

=== gcc -m32 -ffreestanding -fno-stack-protector -fno-pic -Wall -Wextra -c 001_kmain.c -o kmain.o ===
(no output = clean compile, zero warnings)

=== ld -m elf_i386 -T 001_linker.ld -o kernel.bin boot.o kmain.o ===
ld: warning: boot.o: missing .note.GNU-stack section implies executable stack
ld: NOTE: This behaviour is deprecated and will be removed in a future version of the linker
ld: warning: kernel.bin has a LOAD segment with RWX permissions

=== grub-file --is-x86-multiboot2 kernel.bin ===
kernel.bin: VALID Multiboot2 image
```

**[COMMON TRAP]** Read that `ld` output again: two real warnings, not zero. `missing .note.GNU-stack section implies executable stack` and `kernel.bin has a LOAD segment with RWX permissions` are `ld` correctly noticing that this binary has no explicit stack-permission metadata and, as a direct result, ends up with one segment that is simultaneously readable, writable, and executable -- a genuine security anti-pattern `ld` is right to flag on any *ordinary* Linux binary. They are not bugs to silence with a flag: a freestanding, pre-paging kernel has no MMU-enforced page permissions to speak of yet regardless of what the ELF header claims, so these two warnings simply do not apply to what `kernel.bin` actually is at this stage. A later chapter, once this book has real paging, revisits this exact warning and gives `.text` genuinely non-writable, non-executable-`.data` protection for real -- worth remembering now, not silencing now.

Turning the linked binary into something GRUB can actually boot means packing it into a bootable ISO alongside a `grub.cfg` GRUB reads at boot time, naming the exact same `kernel.bin` this chapter just linked:

```text
set timeout=0
set default=0

menuentry "Unix OS from Scratch -- Chapter 1" {
    multiboot2 /boot/kernel.bin
    boot
}
```

```bash
mkdir -p isodir/boot/grub
cp kernel.bin isodir/boot/kernel.bin
cp grub.cfg isodir/boot/grub/grub.cfg
grub-mkrescue -o kernel.iso isodir
qemu-system-x86_64 -cdrom kernel.iso -serial stdio -display none -no-reboot -m 256M \
    -device isa-debug-exit,iobase=0xf4,iosize=0x04
```

**Output (cloud sandbox -- real, live-executed boot output, QEMU 8.2.2):**

```text
=== qemu-system-x86_64 -cdrom kernel.iso -serial stdio -display none -no-reboot -m 256M -device isa-debug-exit,iobase=0xf4,iosize=0x04 ===
Unix OS from Scratch -- Chapter 1: kernel entry reached
multiboot2 + nasm + freestanding gcc + ld + qemu pipeline: OK
QEMU real exit code: 1
```

That `Unix OS from Scratch -- Chapter 1: kernel entry reached` line did not come from a print statement running under Linux, macOS, or any operating system at all -- it came from a real x86-64 CPU, real-emulated by QEMU down to actual instruction execution, running a kernel this chapter wrote, talking to a real (emulated) UART chip through raw I/O ports, with no operating system underneath it. `QEMU real exit code: 1` confirms `qemu_exit(0)` reached the end of `kmain` and QEMU's own `isa-debug-exit` device shut the machine down deterministically -- `(0 << 1) | 1 = 1` -- rather than this chapter needing to kill a hung process to get its output back.

## Toolchain and verification discipline for this book

Bare-metal kernel development needs a real bootloader (GRUB), a real hardware emulator (QEMU), and a real assembler (NASM) working together with an ordinary freestanding-C toolchain -- confirmed, this session, on the cloud sandbox: QEMU 8.2.2, NASM 2.16.01, `grub-mkrescue`/`xorriso` 1.5.6 (GRUB 2.12), and `gcc`/`ld` (13.3.0/2.42) with `-m32 -ffreestanding` rather than a separate cross-compiler, since this book's own kernel targets x86, the same architecture the cloud sandbox itself already runs on. The connected device was checked directly, not assumed: it has `grub-mkrescue` but no `qemu-system-x86_64`, no `nasm`, and -- as this series' own sibling Hammer book already found in its own Appendix C -- no root access to install either (`sudo -n true` fails with the same real "no new privileges" error). That makes this book's own verification story different from every sibling book's CPU-vectorized chapters, and closer to Hammer's own CUDA backend or its Appendix C/D: every kernel this book builds is genuinely assembled, compiled, linked, and *booted* -- but only on the cloud sandbox. The device still receives and md5-verifies every source file this book commits, the same cross-machine discipline as always; it simply is not the machine doing the booting.

## Chapter summary

Booting a bare-metal kernel is a real, four-step handoff: BIOS/UEFI firmware to GRUB, GRUB to a hand-written 32-bit entry stub, that stub to the first C function this book wrote. This book does not reimplement the first two steps -- GRUB already does them correctly, for real hardware -- and instead relies on GRUB's one real contract, the Multiboot2 header, whose exact required fields (magic number, header length, a checksum that forces a 32-bit sum of zero, and a type-0/size-8 end tag) were quoted directly from GNU's own Multiboot2 Specification and checked field-by-field against this chapter's own `001_boot.asm`. The entry stub's only job is to establish a stack before calling into C, since none exists yet; the first C code has no `printf`, no heap, and no OS underneath it, so it talks to the COM1 UART directly through raw port I/O, and QEMU's own `-serial stdio` flag turns that into real, capturable, locked text output. The build itself surfaced two real, honest `ld` warnings about executable-stack and RWX-segment permissions -- correctly triggered, and correctly not yet fixable, since this kernel has no paging yet to enforce anything different. This book's own bare-metal work is cloud-sandbox-only going forward, a toolchain-availability limit confirmed directly on the connected device rather than assumed.

## Self-check questions

1. Why does this book use GRUB and the Multiboot2 standard instead of writing its own bootloader from scratch, and what real machine-state guarantees does that choice buy `_start` for free?
2. What three things does the Multiboot2 checksum field's value have to make true when added to `magic`, `architecture`, and `header_length` -- and why does `0x100000000 - x` compute exactly that?
3. Why does `_start` have to set up a stack before it can safely `call kmain`, when an ordinary hosted C program never has to do this itself?
4. Why does this chapter choose the COM1 serial port over the VGA text buffer for its very first kernel output, given this book's own discipline of locking exact, real output into every page?
5. The `ld` build in this chapter produced two real warnings rather than a clean link. Why are they real and correct, and why does this chapter not silence them with a linker flag?

**Worked answers**

1. Writing a BIOS/UEFI-compatible bootloader means real-mode 16-bit assembly, A20-line handling, and a hand-rolled protected-mode switch -- a mostly separate skill from what an operating system itself does once running, and GRUB already solves it correctly for real hardware today. By the time GRUB jumps to `_start`, the CPU is already in 32-bit protected mode with a valid GDT, paging disabled, and interrupts disabled -- guarantees this book gets for free rather than having to establish itself.
2. The sum of `magic + architecture + header_length + checksum` has to be exactly zero, modulo 2^32 (a "32-bit unsigned sum of zero," per the spec quoted in this chapter). `0x100000000` is `2^32`; subtracting `x` (the sum of the other three fields) from it and adding that result back to `x` always produces exactly `2^32`, which truncates to `0` in 32-bit unsigned arithmetic -- exactly the required sum, computed once by the assembler at build time.
3. A hosted C program's stack is set up by the OS and its C runtime (`_start` in glibc, long before `main` runs) before any of the programmer's own code executes. This book's own `_start` runs the instant GRUB hands off control, with `esp` left at whatever value GRUB happened to leave it -- not a safe place to build a call frame -- so `_start` has to point `esp` at real, reserved memory (`stack_top`, in `.bss`) itself before `call kmain` can work at all.
4. The VGA text buffer would require a screenshot of QEMU's virtual monitor to verify, which cannot be locked into this page as exact, checkable text the way this book's sibling series always has. QEMU's `-serial stdio` flag turns the COM1 UART's real output into plain terminal text instead, letting this chapter capture and lock in the kernel's exact real output, byte for byte, the same discipline the rest of this book's own series follows.
5. `missing .note.GNU-stack section` and `LOAD segment with RWX permissions` are `ld` correctly noticing that this binary has no stack-permission metadata and ends up with one segment that is readable, writable, and executable at once -- a real anti-pattern on an ordinary Linux binary. But this kernel has no paging yet, so there is no MMU-enforced page-permission system for those ELF-level flags to actually control regardless of what they claim; silencing the warning would hide a real signal without fixing anything, since the actual fix (real, enforced page permissions) does not exist until a later chapter builds paging.
