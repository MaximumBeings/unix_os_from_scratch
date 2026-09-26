/* Chapter 25's own driver, upgraded this chapter from polled to real
 * interrupt-driven waits -- see 026_rtl8139.h's own top-of-file
 * comment for the full citation of both real sources this file is
 * built from and exactly what changed and why. */

#include <stdint.h>

#include "026_pci.h"
#include "026_pic.h"
#include "026_pmm.h"
#include "026_printf.h"
#include "026_rtl8139.h"

/* Real I/O register offsets, relative to this device's own real BAR0
 * I/O base -- cited directly, OSDev Wiki's own "RTL8139" page. */
#define REG_MAC0     0x00u  /* 6 real bytes: this device's own burnt-in MAC */
#define REG_TSD0     0x10u  /* Transmit Status of Descriptor 0 (32-bit) */
#define REG_TSAD0    0x20u  /* Transmit Start Address of Descriptor 0 (32-bit) */
#define REG_RBSTART  0x30u  /* Receive (Rx) Buffer Start Address (32-bit) */
#define REG_CMD      0x37u  /* Command register (8-bit) */
#define REG_IMR      0x3Cu  /* Interrupt Mask Register (16-bit) */
#define REG_ISR      0x3Eu  /* Interrupt Status Register (16-bit) */
#define REG_TCR      0x40u  /* Transmit Configuration Register (32-bit) --
                              * offset cited from the real Realtek
                              * datasheet (0040h), not OSDev's own page,
                              * which never mentions TCR at all. */
#define REG_RCR      0x44u  /* Receive Configuration Register (32-bit) */
#define REG_CONFIG1  0x52u  /* CONFIG_1 register (8-bit) */

/* Command register bits, offset 0x37 -- cited directly, both real
 * sources agree exactly: RST bit 4, RE bit 3, TE bit 2. */
#define CMD_RST 0x10u
#define CMD_RE  0x08u
#define CMD_TE  0x04u

/* ISR/IMR bits this driver actually waits on -- cited directly,
 * OSDev's own page: "Write 0x0005 to the IMR register... TOK and
 * ROK". ROK (Receive OK) is bit 0, TOK (Transmit OK) is bit 2 --
 * 0x1 | 0x4 = 0x5, exactly the cited value. */
#define ISR_ROK 0x01u
#define ISR_TOK 0x04u
#define IMR_ROK_TOK 0x05u

/* Receive Configuration Register bits, offset 0x44 -- cited directly
 * from the real Realtek datasheet (OSDev's own page names AB/AM/APM/
 * AAP/WRAP but never gives their exact bit positions): AAP
 * (Accept All Packets) bit 0, APM (Accept Physical Match) bit 1, AM
 * (Accept Multicast) bit 2, AB (Accept Broadcast) bit 3, WRAP bit 7.
 * This driver writes all five, matching OSDev's own cited init value
 * exactly: "0xf | (1 << 7)" = AAP|APM|AM|AB (0xF) plus WRAP (0x80) =
 * 0x8F. RBLEN (bits 12-11, the real Rx ring size select) is left at
 * its reset value 00 -- "8k + 16 byte", cited directly from the real
 * datasheet -- matching the real physical buffer this driver actually
 * allocates below. */
#define RCR_INIT_VALUE 0x8Fu

/* Transmit Configuration Register bits, offset 0x40 -- this register,
 * and this chapter's own real hardware loopback mode, exist ONLY in
 * the real Realtek datasheet; OSDev's own "RTL8139" page never
 * mentions TCR at all. Cited directly: "18, 17 R/W LBK1, LBK0
 * Loopback test... 00: normal operation... 11: Loopback mode" -- both
 * bits set selects loopback, (0b11 << 17) = 0x60000. This is the
 * real, cited reason this chapter's own demo can prove a real frame
 * was genuinely sent and received without ever touching a real wire:
 * the NIC itself, not this driver, routes transmitted data straight
 * back to its own receiver. */
#define TCR_LOOPBACK_ON 0x60000u

/* This chapter's own real physical DMA buffers -- both allocated
 * through Chapter 7's own pmm_alloc_frame(), the same real physical-
 * frame allocator every other DMA-capable structure in this kernel
 * already uses, and both, like every physical frame this allocator
 * has ever handed out, addressable directly as ordinary pointers:
 * Chapter 8's own paging_init() identity-maps this kernel's whole
 * 0-64 MiB managed range, so a frame's physical address is already a
 * valid virtual one. */
static uint32_t tx_buf_phys = 0;
static uint32_t rx_buf_phys = 0;

static uint16_t io_base = 0;
static uint8_t nic_bus = 0, nic_device = 0, nic_function = 0;

/* This chapter's own new real interrupt state -- `volatile` because
 * every one of these is written from 026_irq11.asm's own stub (via
 * irq11_handler() below), asynchronously, from this exact same CPU's
 * own interrupt context, and read from rtl8139_send()/
 * rtl8139_receive_first_packet()'s own real hlt-wait loops below. This
 * kernel is single-CPU, so no lock is needed for a single-writer,
 * single-reader flag like this -- unlike 013_spinlock.h's own real
 * critical sections, there is no genuine race here to protect against,
 * only the compiler's own right to cache a non-volatile value in a
 * register across loop iterations, which `volatile` forbids. */
static volatile uint8_t rok_pending = 0;
static volatile uint8_t tok_pending = 0;
static volatile uint32_t irq_count = 0;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline uint32_t *phys_ptr(uint32_t phys_addr) {
    return (uint32_t *) phys_addr;
}

int rtl8139_init(void) {
    struct pci_device dev;
    if (!pci_find_by_class(PCI_CLASS_NETWORK, PCI_SUBCLASS_ETHERNET, &dev)) {
        kprintf("rtl8139_init: no real Ethernet controller found on this machine's own PCI "
                "bus -- refusing\n");
        return 0;
    }
    nic_bus = dev.bus;
    nic_device = dev.device;
    nic_function = dev.function;
    kprintf("rtl8139_init: found real device %u:%u.%u, vendor=%x device=%x\n",
            (unsigned) nic_bus, (unsigned) nic_device, (unsigned) nic_function,
            (unsigned) dev.vendor_id, (unsigned) dev.device_id);

    /* Enable real "I/O Space" (bit 0) and "Bus Master" (bit 2) in the
     * PCI Command register (configuration-space offset 0x04), cited
     * directly (OSDev Wiki's own "PCI" page). Offset 0x04's own dword
     * packs real Status (bits 31-16) over real Command (bits 15-0), so
     * this is a real read-modify-write -- OR the two new bits into
     * the low 16 bits, leave the high 16 (Status) exactly as read. */
    uint32_t command_dword = pci_config_read_dword(nic_bus, nic_device, nic_function, 0x04);
    command_dword |= 0x1u | 0x4u;
    pci_config_write_dword(nic_bus, nic_device, nic_function, 0x04, command_dword);

    /* This device's own real BAR0, cited directly (OSDev Wiki's own
     * "PCI" page, I/O Space BAR layout): bit 0 is always 1 for an I/O
     * BAR, bit 1 is reserved, and "you calculate (BAR[x] &
     * 0xFFFFFFFC)" to get the real base address -- this exact device
     * is I/O-space, not memory-mapped (confirmed directly in Chapter
     * 24's own real QEMU monitor capture: "BAR0: I/O at 0xc000"). */
    uint32_t bar0 = pci_config_read_dword(nic_bus, nic_device, nic_function, 0x10);
    io_base = (uint16_t) (bar0 & 0xFFFFFFFCu);
    kprintf("rtl8139_init: real I/O base = 0x%x\n", (unsigned) io_base);

    /* This chapter's own new step: read this device's own real PCI
     * Interrupt Line register, configuration-space offset 0x3C, bits
     * 7-0 -- cited directly, OSDev Wiki's own "PCI" page: "Specifies
     * which input of the system interrupt controllers the device's
     * interrupt pin is connected to... For the x86 architecture this
     * register corresponds to the PIC IRQ numbers 0-15... and a value
     * of 0xFF defines no connection." Offset 0x3C's own dword packs
     * Interrupt Pin (bits 15-8) over Interrupt Line (bits 7-0), so
     * only the low byte is this driver's own real IRQ number. Verified
     * against RTL8139_EXPECTED_IRQ rather than trusted blindly -- see
     * 026_rtl8139.h's own top-of-file comment for why this chapter's
     * own fixed compile-time IDT wiring makes that verification a real
     * requirement, not a formality. */
    uint32_t irq_dword = pci_config_read_dword(nic_bus, nic_device, nic_function, 0x3Cu);
    uint8_t real_irq = (uint8_t) (irq_dword & 0xFFu);
    kprintf("rtl8139_init: real PCI Interrupt Line register reports IRQ %u\n",
            (unsigned) real_irq);
    if (real_irq != RTL8139_EXPECTED_IRQ) {
        kprintf("rtl8139_init: this driver's own IDT gate is only ever wired for IRQ %u -- "
                "refusing rather than silently waiting on an interrupt that can never arrive\n",
                (unsigned) RTL8139_EXPECTED_IRQ);
        return 0;
    }

    /* This chapter's own two real physical DMA buffers. The transmit
     * buffer needs only one real 4 KiB frame -- RTL8139_MAX_FRAME
     * (1792 bytes) comfortably fits. The receive ring needs the real
     * minimum OSDev's own page cites for RBLEN's reset value, "8192 +
     * 16 (8K + 16 bytes)", PLUS the real extra padding the same page
     * cites for the WRAP bit this driver also sets: "If WRAP is 1...
     * the buffer must be an additional 1500 bytes" -- 8192 + 16 + 1500
     * = 9708 bytes, rounded up to three whole 4 KiB frames (12288
     * bytes). pmm_alloc_frame() itself only ever hands out ONE frame
     * at a time, so this driver allocates three in a row and verifies,
     * for real, that they came back physically CONTIGUOUS -- required
     * for one linear DMA ring, and never merely assumed. */
    tx_buf_phys = pmm_alloc_frame();

    uint32_t rx_frame_0 = pmm_alloc_frame();
    uint32_t rx_frame_1 = pmm_alloc_frame();
    uint32_t rx_frame_2 = pmm_alloc_frame();
    if (rx_frame_1 != rx_frame_0 + 4096u || rx_frame_2 != rx_frame_1 + 4096u) {
        kprintf("rtl8139_init: this kernel's own next three free physical frames were not "
                "contiguous (got 0x%x, 0x%x, 0x%x) -- refusing rather than building a real DMA "
                "ring across a real gap\n",
                (unsigned) rx_frame_0, (unsigned) rx_frame_1, (unsigned) rx_frame_2);
        return 0;
    }
    rx_buf_phys = rx_frame_0;
    kprintf("rtl8139_init: real tx buffer at 0x%x, real rx ring at 0x%x (3 contiguous frames, "
            "verified)\n", (unsigned) tx_buf_phys, (unsigned) rx_buf_phys);

    /* The real cited init sequence, OSDev Wiki's own "RTL8139" page,
     * with this chapter's own new loopback step inserted in the
     * natural place (after RCR, before RE/TE are enabled -- the real
     * Realtek datasheet gives no ordering requirement either way, and
     * this order matches every other register this driver configures
     * before it starts moving real data). */

    /* 1. Power on: "Send 0x00 to the CONFIG_1 register (0x52)". */
    outb((uint16_t) (io_base + REG_CONFIG1), 0x00u);

    /* 2. Software reset: write RST, poll until the real hardware
     * clears it -- this book's own established unconditional-poll
     * convention, unchanged since Chapter 19's own ATA driver. */
    outb((uint16_t) (io_base + REG_CMD), CMD_RST);
    while (inb((uint16_t) (io_base + REG_CMD)) & CMD_RST) {
        /* real hardware clears RST itself once reset completes */
    }

    /* 3. Real receive buffer's own real physical base address. */
    outl((uint16_t) (io_base + REG_RBSTART), rx_buf_phys);

    /* 4. "Write 0x0005 to IMR (0x3C) for TOK and ROK." Chapter 25 set
     * this even though it never installed a real IRQ handler, since
     * ISR's own real bits latch regardless; this chapter is what
     * actually makes IMR's own real job -- deciding which latched ISR
     * bits are allowed to raise a real interrupt at all -- matter for
     * the first time in this book. */
    outw((uint16_t) (io_base + REG_IMR), IMR_ROK_TOK);

    /* 5. Real Receive Configuration -- AAP|APM|AM|AB|WRAP, cited
     * directly and explained above REG_RCR's own definition. */
    outl((uint16_t) (io_base + REG_RCR), RCR_INIT_VALUE);

    /* 6. "Write 0x0C to CMD (0x37)" -- real RE|TE, enabling the real
     * receiver and transmitter DMA engines. */
    outb((uint16_t) (io_base + REG_CMD), CMD_RE | CMD_TE);

    /* 7. This chapter's own new step: real hardware loopback mode,
     * cited directly from the real Realtek datasheet (TCR bits 18/17,
     * "11: Loopback mode") -- the reason this chapter's own demo below
     * can prove real send/receive without a real wire at all. Written
     * LAST, after RE/TE, not in TCR's own natural numeric position
     * next to RCR -- neither real cited source (OSDev's own page,
     * which never mentions TCR at all, or the real Realtek datasheet,
     * which gives no ordering requirement for any of these seven
     * writes) documents this ordering requirement. It was found the
     * same way this book finds everything it cannot look up: by
     * testing for real, in this exact QEMU environment. Writing TCR's
     * own real loopback bits before RE/TE were enabled produced a
     * real, silent failure -- the write appeared to succeed, but a
     * follow-up real readback (this driver's own diagnostic, not
     * shown in the final version below) proved the loopback bits
     * never actually took hold until this exact order was used. */
    outl((uint16_t) (io_base + REG_TCR), TCR_LOOPBACK_ON);

    /* 8. This chapter's own new step: unmask this device's own real
     * IRQ line so it can actually reach the CPU. Cited directly, the
     * real prerequisite 026_pic.c's own pic_disable_all() left every
     * one of the 15 usable lines masked, and no earlier chapter ever
     * unmasked line 2 or any line 8-15 -- OSDev Wiki's own "8259 PIC"
     * page: "Masking IRQ2 will cause the Slave PIC to stop raising
     * IRQs," since IRQ 2 on the master PIC is the real cascade line
     * every slave-PIC line (8-15) physically routes through. Both
     * unmasks are needed, in either order; this driver does the
     * cascade line first since that is the one every future slave-PIC
     * driver in this book will also need, RTL8139-specific or not. */
    pic_clear_mask(2);
    pic_clear_mask(RTL8139_EXPECTED_IRQ);

    kprintf("rtl8139_init: real device brought up, real hardware loopback mode enabled, "
            "real IRQ %u unmasked\n", (unsigned) RTL8139_EXPECTED_IRQ);
    return 1;
}

void rtl8139_get_mac(uint8_t out_mac[6]) {
    for (uint32_t i = 0; i < 6u; i++) {
        out_mac[i] = inb((uint16_t) (io_base + REG_MAC0 + i));
    }
}

int rtl8139_send(const uint8_t *frame, uint32_t len) {
    if (len > RTL8139_MAX_FRAME) {
        kprintf("rtl8139_send: %u bytes exceeds this real device's own real 1792-byte maximum "
                "-- refusing\n", (unsigned) len);
        return 0;
    }

    uint8_t *tx_buf = (uint8_t *) phys_ptr(tx_buf_phys);
    for (uint32_t i = 0; i < len; i++) {
        tx_buf[i] = frame[i];
    }

    /* Real transmit descriptor 0: its own real physical start address,
     * then its own real length -- writing TSD0 with the real OWN bit
     * (bit 13) left clear is what genuinely triggers transmission,
     * cited directly (OSDev Wiki's own "RTL8139" page). This
     * chapter's own minimal scope only ever sends one frame at a
     * time, so descriptor 0 alone is always correct -- round-robin
     * selection across the real TSD0-3/TSAD0-3 pairs is left for a
     * future chapter that needs more than one frame in flight.
     *
     * Both flags are cleared here, not just `tok_pending` -- this
     * chapter's own real hardware loopback mode (Chapter 25's own
     * TCR bits) delivers a looped-back frame into the receive ring as
     * a genuinely SYNCHRONOUS side effect of this exact transmission,
     * confirmed directly during Chapter 25's own real debugging of
     * this exact QEMU environment. That means `rok_pending` can
     * already be 1 by the time this function returns, before
     * rtl8139_receive_first_packet() is ever called -- clearing it
     * here, immediately before the real transmit that can trigger it,
     * is what keeps that real event from being silently lost. */
    tok_pending = 0;
    rok_pending = 0;
    outl((uint16_t) (io_base + REG_TSAD0), tx_buf_phys);
    outl((uint16_t) (io_base + REG_TSD0), len);

    /* This chapter's own new real interrupt-driven wait, replacing
     * Chapter 25's own tight TSD0 spin: `hlt` genuinely stops this CPU
     * until the NEXT real interrupt of ANY kind, not only this
     * device's own -- exactly like every other `hlt` in this book
     * since Chapter 12's own preemptive scheduler, this kernel's own
     * real IRQ0 tick (still firing throughout, unchanged) will also
     * wake it, in which case this loop simply finds `tok_pending`
     * still 0 and goes back to sleep. `tok_pending` is set to 1 only
     * by irq11_handler() below, only once this device's own real ISR
     * has actually reported TOK. */
    while (!tok_pending) {
        __asm__ volatile ("hlt");
    }

    return 1;
}

int rtl8139_receive_first_packet(uint8_t *out_buf, uint32_t *out_len) {
    /* This chapter's own new real interrupt-driven wait, replacing
     * Chapter 25's own tight ISR spin -- see rtl8139_send()'s own
     * comment above for why a real IRQ0 tick waking this `hlt` early
     * is expected and harmless, not a bug. `rok_pending` is set to 1
     * only by irq11_handler() below, which -- cited directly, OSDev
     * Wiki's own "RTL8139" page: "you have to write the bit
     * corresponding to the interrupt to reset it" -- has ALREADY
     * acknowledged the real ISR bit by the time this loop ever sees
     * `rok_pending` become 1, unlike Chapter 25's own version, which
     * acknowledged it here, after its own poll noticed it.
     *
     * Deliberately NOT cleared to 0 here before waiting, unlike
     * `tok_pending` in rtl8139_send() above -- rtl8139_send()'s own
     * comment explains why this exact real loopback frame's own ROK
     * can already have arrived, synchronously, before this function is
     * even called; resetting it here would silently discard that real
     * event and wait forever for one that will never come a second
     * time. It is cleared instead at the end of this function, once
     * this exact real packet has actually been consumed. */
    while (!rok_pending) {
        __asm__ volatile ("hlt");
    }

    /* This chapter's own stated, cited scope limit (026_rtl8139.h's
     * own top-of-file comment): neither real source documents the
     * real CAPR-update procedure, so this driver never advances CAPR
     * and never reads a second real packet -- it reads exactly the
     * FIRST packet a freshly reset ring ever receives, directly from
     * the ring's own known real base address, `rx_buf_phys`. Every
     * received packet begins with a real 4-byte header -- a 16-bit
     * status word, then a 16-bit length -- cited directly (OSDev
     * Wiki's own "RTL8139" page), followed immediately by the real
     * packet data itself. */
    const uint8_t *rx_buf = (const uint8_t *) phys_ptr(rx_buf_phys);
    uint16_t status = (uint16_t) (rx_buf[0] | (rx_buf[1] << 8));
    uint16_t length = (uint16_t) (rx_buf[2] | (rx_buf[3] << 8));

    if ((status & ISR_ROK) == 0) {
        kprintf("rtl8139_receive_first_packet: real packet header status 0x%x does not report "
                "ROK -- refusing\n", (unsigned) status);
        return 0;
    }

    uint32_t copy_len = length;
    if (copy_len > RTL8139_MAX_FRAME) {
        copy_len = RTL8139_MAX_FRAME;
    }
    for (uint32_t i = 0; i < copy_len; i++) {
        out_buf[i] = rx_buf[4u + i];
    }
    *out_len = copy_len;

    /* This exact real packet has now genuinely been consumed --
     * cleared here, not before the wait above, for the real reason
     * explained in that comment. */
    rok_pending = 0;

    return 1;
}

/* This chapter's own new real interrupt handler -- see
 * 026_rtl8139.h's own top-of-file comment for the full citation of
 * the real ISR-acknowledgment requirement this handler implements.
 * Called from 026_irq11.asm's own stub, which -- exactly like every
 * other IRQ stub in this book since Chapter 5's own irq1 -- has
 * already pushed every general-purpose register by the time this
 * function runs, and will pop them all and IRET once this function
 * returns. */
void irq11_handler(void) {
    irq_count++;

    /* One real register read gets every real ISR bit this exact
     * interrupt is reporting, latched together if more than one real
     * event happened close enough together to share one real
     * delivery (cited directly, OSDev Wiki's own "RTL8139" page: "The
     * IMR bits line up with the ISR bits to work in sync") -- writing
     * that same value straight back is what acknowledges every one of
     * them at once, cited directly in this file's own top-of-file
     * citation of the same page's own interrupt-handling guidance. */
    uint16_t status = inw((uint16_t) (io_base + REG_ISR));
    outw((uint16_t) (io_base + REG_ISR), status);

    if (status & ISR_ROK) {
        rok_pending = 1;
    }
    if (status & ISR_TOK) {
        tok_pending = 1;
    }

    /* This device's own real IRQ line is line 11, one of the eight
     * slave-PIC lines (8-15) -- cited directly, 026_pic.c's own
     * pic_send_eoi(): "For master-originated IRQs, write to the
     * master command port only; for slave IRQs, it is necessary to
     * issue the command to both PIC chips," which pic_send_eoi()
     * itself already implements for any `irq_line >= 8`. */
    pic_send_eoi(RTL8139_EXPECTED_IRQ);
}

uint32_t rtl8139_get_irq_count(void) {
    return irq_count;
}
