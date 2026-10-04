#!/usr/bin/env python3
"""Writes one picture per chapter (ch-01.svg ... ch-46.svg) and per appendix (appx-a.svg ... appx-d.svg) into this directory. The theme of each picture follows the chapter's own subject: a power button for
booting, a clock for the timer, a bank for a wire transfer, an umbrella for insurance, an airplane for flights, ... Run:  python3 make_art.py"""
import os, sys
here = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, here)
from lib import Svg
import scenes1, scenes2
S = {"hero": scenes2.hero, **{f"s{n:02d}": getattr(scenes1, f"s{n:02d}") for n in range(1, 24)}, **{f"s{n:02d}": getattr(scenes2, f"s{n:02d}") for n in range(24, 51)}, **{k: getattr(scenes2, k) for k in ("sA", "sB", "sC", "sD")}}
# (palette, alt text) per chapter
CH = {
 1: ("hw", "A power button, a bootloader and a CPU: booting a Multiboot2 kernel"), 2: ("screen", "A text-mode monitor with coloured character cells at address 0xB8000"),
 3: ("hw", "A global descriptor table and a printf call"), 4: ("irq", "A bell ringing at a CPU: an interrupt reaching the interrupt descriptor table"),
 5: ("irq", "A keyboard sending scancodes over IRQ 1"), 6: ("time", "A clock and a train of timer ticks"), 7: ("mem", "A grid of 4 KiB physical memory frames, some used"),
 8: ("mem", "Virtual pages mapped to physical frames through a page directory"), 9: ("mem", "A heap of used and free blocks with kmalloc and kfree"),
 10: ("lock", "A warning triangle and the page-fault exception"), 11: ("task", "Four tasks passing control to each other with yield"), 12: ("task", "A timer interrupt switching round-robin between four tasks"),
 13: ("lock", "A padlock and a spinning arrow: a spinlock"), 14: ("task", "A semaphore flag and sleeping tasks in a wait queue"), 15: ("ring", "Concentric protection rings and a system call"),
 16: ("ring", "Several ring-3 user tasks around the kernel"), 17: ("mem", "Three processes, each with its own page directory"), 18: ("disk", "An ELF file whose segments are loaded into memory"),
 19: ("disk", "A spinning disk platter and an ATA read head"), 20: ("disk", "A FAT16 cluster chain leading to a file"), 21: ("disk", "A root folder with subfolders and files"),
 22: ("disk", "An empty folder crossed out: removing a directory"), 23: ("disk", "A deep path of nested folders ending in a file"), 24: ("hw", "A PCI bus with a network card, a GPU and other devices"),
 25: ("net", "A network card sending and receiving an Ethernet frame"), 26: ("net", "A sleeping CPU woken by a network interrupt"), 27: ("net", "A ring of eight receive buffers and a wrapping pointer"),
 28: ("net", "A broadcast asking who has an IP address: ARP"), 29: ("net", "An ARP cache table of IP and MAC addresses"), 30: ("bank", "Two banks exchanging an encrypted wire transfer"),
 31: ("net", "A server answering an ARP request about its own address"), 32: ("bank", "A restaurant bill split four ways and sent as an ACH batch"), 33: ("bank", "A purchase paid in four instalments"),
 34: ("ins", "An umbrella and three insurance carriers' quotes"), 35: ("invest", "A rising investment chart and a rounded-up purchase"), 36: ("invest", "A budget pie chart and a cash-flow line"),
 37: ("media", "A video player and the segments of an adaptive stream"), 38: ("atm", "A cash machine with a PIN pad dispensing banknotes"), 39: ("atm", "A card terminal and an EMV chip card"),
 40: ("bank", "An invoice and a monthly billing calendar"), 41: ("travel", "An airplane on a route between two airports with competing fares"), 42: ("sport", "A match ticket with a barcode and a rotating code"),
 43: ("bet", "A betting odds board and casino chips"), 44: ("car", "A rental car and its key"), 45: ("net", "An IP header, an ICMP echo and ping ripples"), 46: ("irq", "Five numbered doors: gates installed in the interrupt descriptor table"),
 50: ("btc", "Three blocks chained by hash, a Merkle tree of transactions, and a block hash that must fall below the target: a Bitcoin block validator"),
 49: ("health", "An X12 837 claim form, an adjudication ledger splitting a charge into plan, patient and write-off, and a balanced 835 remittance"),
 48: ("edgar", "A Form 10-K filing, the XBRL tags read from it, a balance scale of assets against liabilities plus equity, and the ratios computed from it"),
 47: ("auction", "A lamp on a pedestal, an auctioneer's gavel, a ladder of rising bids and a SOLD stamp: an eBay-style auction"),
}
APPX = {"a": ("ref", "sA", "x86 registers, assembly instructions and the machine code they become"), "b": ("ref", "sB", "A Unix terminal with a pipeline and a build command"),
        "c": ("ref", "sC", "C source code between large curly braces"), "d": ("ref", "sD", "Check marks beside question and answer boxes")}
n = 0
for name, pal, fn, alt in (("hero", "hw", "hero", "A layered stack from hardware and boot code through the kernel up to a bank, an ATM, an umbrella and an airplane: a kernel built from nothing, and the applications on it"),
                           ("start", "ref", "sB", "A terminal with a build command: getting started")):
    s = Svg(pal, alt); S[fn](s); open(os.path.join(here, name + ".svg"), "w").write(s.svg()); n += 1
for k, (pal, alt) in CH.items():
    s = Svg(pal, alt); S[f"s{k:02d}"](s); open(os.path.join(here, f"ch-{k:02d}.svg"), "w").write(s.svg()); n += 1
for L, (pal, fn, alt) in APPX.items():
    s = Svg(pal, alt); S[fn](s); open(os.path.join(here, f"appx-{L}.svg"), "w").write(s.svg()); n += 1
print("wrote", n, "pictures")
