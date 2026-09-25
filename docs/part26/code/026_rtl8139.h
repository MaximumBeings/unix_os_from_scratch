#ifndef UNIX_OS_026_RTL8139_H
#define UNIX_OS_026_RTL8139_H

#include <stdint.h>

/* Chapter 25's own driver for the real RTL8139 Fast Ethernet
 * controller Chapter 24's own pci_find_by_class() first located, by
 * class code alone, on this exact machine's own real PCI bus. Register
 * offsets, bit meanings, and the real initialization sequence are
 * still cited field-for-field from the same two real sources: OSDev
 * Wiki's own "RTL8139" page (https://wiki.osdev.org/RTL8139), and,
 * wherever that page is silent, the real manufacturer datasheet it is
 * itself derived from -- REALTEK RTL8139D(L), Rev. 1.11, 2001/11/09
 * (https://www.cs.usfca.edu/~cruse/cs326f04/RTL8139D_DataSheet.pdf).
 *
 * This chapter's own new work: Chapter 25's driver was explicitly,
 * stated-scope POLLED -- rtl8139_send() and
 * rtl8139_receive_first_packet() each spun on a hardware register
 * (TSD0, ISR) in a tight loop until the real hardware set the bit they
 * were waiting for. This chapter replaces both spins with this book's
 * first real interrupt-driven device wait: a real PCI Interrupt Line
 * lookup (OSDev Wiki's own "PCI" page, configuration-space offset
 * 0x3C, bits 7-0), a real new IDT gate and 8259 PIC unmask for that
 * exact line, a real interrupt handler that acknowledges the ISR bits
 * cited exactly as Chapter 25 already did, and a real `hlt` in place
 * of the old spin -- the CPU is genuinely asleep, not burning cycles
 * re-reading a port, between the request and the real hardware event
 * that answers it.
 *
 * Every scope limit Chapter 25 stated still holds and is unchanged by
 * this chapter: no more than one frame in flight at a time; no CAPR
 * advancement or receive-ring wraparound (this driver still only ever
 * reads the very first packet a freshly reset ring receives); and the
 * device's own real hardware loopback mode is still what this chapter's
 * own demo uses to prove a real send/receive round trip without a real
 * wire. This chapter's OWN new scope limit, stated here the same way:
 * this kernel's IDT gates are installed once, at compile time, in
 * 026_idt.c's own idt_init() -- there is no general mechanism in this
 * book (yet) for installing a NEW gate at runtime once a driver
 * discovers which real IRQ line its device landed on. This driver
 * works around that honestly, not silently: it reads the real
 * Interrupt Line register at runtime and VERIFIES it against the one
 * fixed vector idt_init() already wired up for it (vector 0x2B, IRQ
 * 11 -- confirmed, independently of this driver's own code, by
 * Chapter 24's own real QEMU monitor capture: "IRQ 11, pin A"),
 * refusing rather than silently pretending to be interrupt-driven if
 * the real hardware ever reports something this exact wiring can't
 * actually receive. A future chapter that needs a real dynamic IDT is
 * the natural place to lift that limit. */

/* The largest single frame this driver will ever send or receive,
 * cited directly (Realtek datasheet / OSDev's own page): a real
 * RTL8139 transmit descriptor accepts at most 1792 bytes per frame. */
#define RTL8139_MAX_FRAME 1792u

/* This chapter's own real, fixed IRQ wiring -- see this file's own
 * top-of-file comment for why it is fixed rather than discovered and
 * installed dynamically. Also the real IDT vector that IRQ maps to
 * under this kernel's own real 8259 remap (026_kmain.c calls
 * `pic_remap(0x20, 0x28)`, so IRQ 11 -- the ninth slave-PIC line,
 * 11 - 8 = 3 -- lands on vector 0x28 + 3 = 0x2B). */
#define RTL8139_EXPECTED_IRQ 11u
#define RTL8139_IDT_VECTOR   0x2Bu

/* Locates the real RTL8139 on this machine's own PCI bus (via
 * Chapter 24's own pci_find_by_class()), enables real PCI I/O-space
 * and bus-mastering access, reads the device's own real BAR0 to learn
 * its real I/O base, reads its own real PCI Interrupt Line register
 * and verifies it against RTL8139_EXPECTED_IRQ (this chapter's own new
 * step -- see top-of-file comment), allocates this driver's own real
 * physical transmit and receive buffers (via Chapter 7's own
 * pmm_alloc_frame()), runs the real cited power-on/reset/configure
 * sequence, unmasks this device's own real IRQ line (and the real
 * cascade line, IRQ 2, that a slave-PIC line needs to ever reach the
 * CPU at all -- cited directly, OSDev Wiki's own "8259 PIC" page:
 * "Masking IRQ2 will cause the Slave PIC to stop raising IRQs"), and
 * ends with the device's own real hardware loopback mode enabled.
 * Returns 1 on success, 0 if no real RTL8139 was found, or if its real
 * reported IRQ does not match this chapter's own fixed wiring. */
int rtl8139_init(void);

/* Copies this device's own real, burnt-in 6-byte station address --
 * read directly from real registers MAC0-5 -- into `out_mac`. */
void rtl8139_get_mac(uint8_t out_mac[6]);

/* Sends exactly one real frame, `len` bytes (at most
 * RTL8139_MAX_FRAME), via this device's own real transmit descriptor
 * 0. This chapter's own new behavior: blocks with a real `hlt` per
 * iteration, woken only by a real interrupt (this device's own real
 * IRQ, or -- exactly like every other `hlt` in this book since
 * Chapter 12 -- this kernel's own real IRQ0 tick, which simply loops
 * back and re-checks), instead of Chapter 25's own tight TSD0 spin.
 * Returns 1 on success. */
int rtl8139_send(const uint8_t *frame, uint32_t len);

/* Blocks the same real interrupt-driven way as rtl8139_send() above,
 * woken by this device's own real ROK interrupt instead of Chapter
 * 25's own tight ISR spin, then copies the real received frame --
 * still read directly from this ring's own known base address, the
 * FIRST and only packet this driver ever reads back, cited and
 * reasoned through in this file's own top-of-file comment and
 * unchanged since Chapter 25 -- into `out_buf` (must be at least
 * RTL8139_MAX_FRAME bytes). Writes the real received length into
 * `*out_len`. Returns 1 on success. */
int rtl8139_receive_first_packet(uint8_t *out_buf, uint32_t *out_len);

/* This chapter's own new real interrupt handler, called from
 * 026_irq11.asm's own stub on every real IRQ 11. Acknowledges
 * whichever real ISR bits are actually set -- cited directly, OSDev
 * Wiki's own "RTL8139" page: "When you handle an interrupt, you have
 * to write the bit corresponding to the interrupt to reset it," and
 * this must happen "before you read any packets from your buffers, or
 * the write to the register will have no effect" -- records which real
 * event(s) this exact interrupt reported into this driver's own
 * internal flags for rtl8139_send()/rtl8139_receive_first_packet() to
 * observe, sends this device's own real end-of-interrupt, and returns.
 * Not intended to be called from anywhere but 026_irq11.asm's own stub. */
void irq11_handler(void);

/* This chapter's own new real, live interrupt counter -- incremented
 * once per real IRQ 11 delivery, entirely independent of whether that
 * delivery turned out to carry ROK, TOK, both, or (in principle)
 * neither. Exists purely so this chapter's own demo, and this
 * chapter's own independent verification, can report and check a
 * real, concrete number rather than merely asserting "an interrupt
 * fired" -- the same "count it and check it" verification style this
 * book has used since Chapter 11/12's own real context-switch counts. */
uint32_t rtl8139_get_irq_count(void);

#endif
