#ifndef UNIX_OS_025_RTL8139_H
#define UNIX_OS_025_RTL8139_H

#include <stdint.h>

/* This chapter's own new driver: the real RTL8139 Fast Ethernet
 * controller Chapter 24's own pci_find_by_class() first located, by
 * class code alone, on this exact machine's own real PCI bus --
 * class 0x02 ("Network Controller"), subclass 0x00 ("Ethernet
 * Controller"). Register offsets, bit meanings, and the real
 * initialization sequence are cited field-for-field from two real
 * sources: OSDev Wiki's own "RTL8139" page (https://wiki.osdev.org/
 * RTL8139), and, wherever that page is silent, the real manufacturer
 * datasheet it is itself derived from -- REALTEK RTL8139D(L), Rev.
 * 1.11, 2001/11/09 (https://www.cs.usfca.edu/~cruse/cs326f04/
 * RTL8139D_DataSheet.pdf) -- the same "reach for a different real,
 * authoritative document when the usual one is silent" pattern this
 * book has followed since Chapter 22.
 *
 * This chapter's own stated, minimal scope: bring the real NIC up,
 * and prove one real Ethernet frame can be sent and received -- via
 * the card's own real, hardware MAC loopback mode (TCR's own real
 * LBK1/LBK0 bits, cited directly from the real Realtek datasheet,
 * since neither OSDev's own page nor the datasheet's own Command/RCR
 * sections mention loopback at all), not a real wire. No interrupt
 * handling (this book's own established polled-driver convention,
 * unchanged since Chapter 19's own ATA driver); no more than one
 * frame in flight at a time; no CAPR advancement or receive-ring
 * wraparound -- this driver reads exactly the FIRST packet a freshly
 * reset ring ever receives, directly from the ring's own known base
 * address, and deliberately does not implement receiving a second
 * one. A future chapter that needs to receive more than one real
 * frame in sequence is the natural place to revisit that limit,
 * cited directly here as a real, stated boundary rather than an
 * accidental one -- neither OSDev's own page nor the real Realtek
 * datasheet actually documents the real CAPR-update procedure at
 * all, confirmed directly against both; deriving one without a real
 * citable source to check it against was exactly the kind of
 * invented, unverifiable behavior this book has avoided since
 * Chapter 1. */

/* The largest single frame this driver will ever send or receive,
 * cited directly (Realtek datasheet / OSDev's own page): a real
 * RTL8139 transmit descriptor accepts at most 1792 bytes per frame. */
#define RTL8139_MAX_FRAME 1792u

/* Locates the real RTL8139 on this machine's own PCI bus (via
 * Chapter 24's own pci_find_by_class()), enables real PCI I/O-space
 * and bus-mastering access, reads the device's own real BAR0 to learn
 * its real I/O base, allocates this driver's own real physical
 * transmit and receive buffers (via Chapter 7's own pmm_alloc_frame(),
 * the same real physical-frame allocator every other DMA-capable
 * structure in this kernel already uses), and runs the real cited
 * power-on/reset/configure sequence, ending with the device's own
 * real hardware loopback mode enabled. Returns 1 on success, 0 if no
 * real RTL8139 was found on this machine's own PCI bus at all. */
int rtl8139_init(void);

/* Copies this device's own real, burnt-in 6-byte station address --
 * read directly from real registers MAC0-5 -- into `out_mac`. */
void rtl8139_get_mac(uint8_t out_mac[6]);

/* Sends exactly one real frame, `len` bytes (at most
 * RTL8139_MAX_FRAME), via this device's own real transmit descriptor
 * 0 -- this chapter's own minimal scope never needs more than one
 * frame in flight, so round-robin descriptor selection across TSD0-3
 * is left for a future chapter. Blocks, polling this device's own
 * real TSD0 register, until the real hardware reports the real frame
 * genuinely sent (TOK, bit 15). Returns 1 on success. */
int rtl8139_send(const uint8_t *frame, uint32_t len);

/* Blocks, polling this device's own real ISR register, until the real
 * hardware reports a real received frame (ROK, bit 0), then copies it
 * -- read directly from this ring's own known base address, the
 * FIRST and only packet this driver ever reads back, cited and
 * reasoned through in this file's own top-of-file comment -- into
 * `out_buf` (must be at least RTL8139_MAX_FRAME bytes). Writes the
 * real received length into `*out_len`. Returns 1 on success. */
int rtl8139_receive_first_packet(uint8_t *out_buf, uint32_t *out_len);

#endif
