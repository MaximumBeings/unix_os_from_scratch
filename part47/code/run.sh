#!/bin/sh
# Usage: run.sh [OUTDIR]  -- boots OUTDIR/os.iso in QEMU with a fresh 8 MiB raw disk and an RTL8139 card, serial to stdout. Exits when the kernel writes to the isa-debug-exit port.
OUT=${1:-build}; rm -f "$OUT/disk.img"; qemu-img create -q -f raw "$OUT/disk.img" 8M
timeout ${TIMEOUT:-60} qemu-system-x86_64 -cdrom "$OUT/os.iso" -drive file="$OUT/disk.img",format=raw,if=ide,index=0 -serial stdio -display none -no-reboot -m 64M \
  -device isa-debug-exit,iobase=0xf4,iosize=0x04 -netdev user,id=n0 -device rtl8139,netdev=n0 || true
