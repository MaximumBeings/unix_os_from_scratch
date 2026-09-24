# Getting Started

This book builds a bare-metal, x86-64-targeted, Unix-family-inspired kernel in C, assembled and linked with an ordinary freestanding toolchain -- no separate cross-compiler is needed, since this book's own kernel targets the same architecture (x86-64/i386-compatible) its cloud authoring machine already runs on. Booting is real: every kernel this book builds is packed into a bootable ISO with GRUB and genuinely booted in QEMU, a real, complete x86 hardware emulator -- not a simulator that approximates behavior, an emulator that actually executes the compiled machine code.

## Installing a toolchain

```bash
nasm -v                    # x86 assembler, needed from Chapter 1 onward
gcc --version               # freestanding C compiler (-ffreestanding), needed from Chapter 1 onward
ld --version                 # linker, needed from Chapter 1 onward
grub-mkrescue --version     # builds a bootable ISO from a Multiboot2 kernel
qemu-system-x86_64 --version # boots that ISO for real
```

On Debian/Ubuntu, all five come from `apt-get install nasm build-essential qemu-system-x86 grub-pc-bin grub-common xorriso mtools`. `gcc`/`ld` are used with `-m32 -ffreestanding` rather than a dedicated cross-compiler; this works because a freestanding kernel needs no host operating system's headers or runtime, and an ordinary Linux `gcc` can already emit i386/x86-64 machine code directly.

## The honesty discipline this book follows

This book's own two authoring machines, confirmed directly rather than assumed: a cloud sandbox (x86_64, with a real, working `nasm`/`gcc`/`ld`/`grub-mkrescue`/`qemu-system-x86_64` toolchain, all version-checked in Chapter 1) and a connected device (an aarch64 macOS-hosted Linux VM, with `gcc` and `grub-mkrescue` but **no** `nasm` and **no** `qemu-system-x86_64` at all -- and no root access to install either, checked directly with `which`/`sudo -n true`, not assumed). The gap runs deeper than those two missing tools: the device's own `gcc` targets aarch64 and genuinely rejects `-m32` outright (`gcc: error: unrecognized command-line option '-m32'`, confirmed directly), so even this book's freestanding **C compile step** cannot run there, not only the assembly and boot steps -- an architecture-target mismatch, not a missing-package one, and not fixable by installing anything. That means, unlike a book whose real hardware-facing work can be cross-verified on two different real machines, every real build-and-boot step in this book happens on the cloud sandbox only:

- **Assembly, compilation, and linking** (NASM, `gcc -ffreestanding`, `ld`) happen for real, and their exact real output -- including real linker warnings, when `ld` produces them -- is captured and locked into the page unedited.
- **Booting** happens for real, in QEMU, a genuine x86 hardware emulator that executes the compiled machine code rather than approximating it; this book's kernels talk to real (emulated) hardware devices -- the serial UART first, more later -- through the same I/O-port instructions a real physical machine would use.
- **Cross-machine verification**, this book's sibling series' own standing discipline, still applies wherever it can: every source file this book commits is still sent to the connected device and verified byte-identical via md5sum, the same as every sibling book. What differs is that the device is not the machine doing the booting, a toolchain-availability limit confirmed directly (not assumed) the same way this author's Hammer compiler book found for its own Appendix C/D.
- **Real facts this book states about x86 architecture, the Multiboot2 standard, or any other real specification** are cited to that specification's own real, current documentation, quoted directly rather than paraphrased from memory wherever a direct quote is practical.

Every chapter states which of the above applies to its own code, so nothing is left for a reader to guess about how a claim in this book was actually established.

## Compile-line conventions

- Assembly (`.asm`, NASM syntax): `nasm -f elf32 file.asm -o file.o` (32-bit protected-mode object files, this book's own convention until a later chapter moves to 64-bit long mode)
- Freestanding C (`.c`): `gcc -m32 -ffreestanding -fno-stack-protector -fno-pic -Wall -Wextra -c file.c -o file.o`
- Linking: `ld -m elf_i386 -T linker.ld -o kernel.bin *.o`, with each chapter's own linker script shown in full where it is introduced
- Packing and booting: each chapter shows its own exact `grub-mkrescue`/`qemu-system-x86_64` invocation, including every flag, inline where the kernel it applies to is introduced

## Prerequisites

This book assumes working knowledge of C (functions, pointers, `struct`s, the `static` keyword) and enough x86 assembly to read a short, heavily-commented file -- no prior assembly-writing experience is assumed, and every instruction this book uses is explained the first time it appears. No prior operating-systems or bare-metal development experience is assumed either; Chapter 1 builds the real boot chain (firmware, bootloader, kernel entry) from nothing before any kernel code is written.
