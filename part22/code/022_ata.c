/* This chapter's first driver for real, persistent storage -- every
 * byte this kernel has ever touched before now lived in RAM, gone the
 * instant QEMU exits. A hard disk is a genuinely different kind of
 * device: this driver talks to it entirely through ordinary port I/O
 * (no memory-mapped registers, no DMA -- Programmable I/O, "PIO"),
 * polling a status register by hand rather than waiting on an IRQ,
 * the simplest of the several real modes OSDev Wiki's own "ATA PIO
 * Mode" page describes. This chapter deliberately stays polled, not
 * interrupt-driven: every IRQ line this kernel has ever used (IRQ0,
 * IRQ1) already has a real handler installed and enabled, but wiring
 * up IRQ14 (the primary ATA controller's own line) would mean a
 * fourth real interrupt source competing for this kernel's attention
 * with no new capability this chapter actually needs -- ata_read_
 * sector()/ata_write_sector() below already return only once the
 * real transfer is complete, exactly the guarantee an interrupt
 * handler would otherwise exist to provide. A later chapter that
 * needs to overlap disk I/O with other work is the natural place to
 * revisit that choice, the same kind of explicit, stated scope limit
 * this book has drawn before (Chapter 5's own PIC-unmask deviation,
 * Chapter 13's own cli/sti-only spinlock). */

#include <stdint.h>

#include "022_ata.h"
#include "022_printf.h"

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
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

/* Primary ATA bus port map, cited field-for-field (OSDev Wiki, "ATA
 * PIO Mode"): "The first two buses are called the Primary and
 * Secondary ATA bus, and are almost always controlled by IO ports
 * 0x1F0 through 0x1F7, and 0x170 through 0x177, respectively... The
 * associated Device Control Registers/Alternate Status ports are IO
 * ports 0x3F6, and 0x376, respectively." This driver only ever
 * touches the primary bus. */
#define ATA_DATA          0x1F0u
#define ATA_ERROR         0x1F1u
#define ATA_SECCOUNT      0x1F2u
#define ATA_LBA_LO        0x1F3u
#define ATA_LBA_MID       0x1F4u
#define ATA_LBA_HI        0x1F5u
#define ATA_DRIVE_SELECT  0x1F6u
#define ATA_STATUS        0x1F7u  /* read */
#define ATA_COMMAND       0x1F7u  /* write */
#define ATA_CONTROL       0x3F6u  /* alternate status (read) / device control (write) */

/* Status register bits, cited the same way: bit 0 "Indicates an error
 * occurred," bit 3 "Set when the drive has PIO data to transfer, or
 * is ready to accept PIO data," bit 5 "Drive Fault Error (does not
 * set ERR)," bit 7 "Indicates the drive is preparing to send/receive
 * data (wait for it to clear)." */
#define ATA_SR_ERR 0x01u
#define ATA_SR_DRQ 0x08u
#define ATA_SR_DF  0x20u
#define ATA_SR_BSY 0x80u

/* Command byte values, cited the same way: IDENTIFY is 0xEC, 28-bit
 * LBA READ SECTORS is 0x20, 28-bit LBA WRITE SECTORS is 0x30, and
 * Cache Flush is 0xE7. */
#define ATA_CMD_IDENTIFY      0xECu
#define ATA_CMD_READ_SECTORS  0x20u
#define ATA_CMD_WRITE_SECTORS 0x30u
#define ATA_CMD_CACHE_FLUSH   0xE7u

/* "Many drives require a little time to respond to a 'select', and
 * push their status onto the bus. The suggestion is to read the
 * Status register FIFTEEN TIMES, and only pay attention to the value
 * returned by the last one -- after selecting a new master or slave
 * device" (OSDev Wiki, "ATA PIO Mode"). Reading the alternate status
 * port (0x3F6) rather than the ordinary one (0x1F7) is deliberate: an
 * alternate-status read never clears a pending interrupt the way an
 * ordinary status read can, so it is always safe to use purely as a
 * delay, whether or not this driver ever enables ATA's own IRQ. */
static void ata_delay_400ns(void) {
    for (int i = 0; i < 15; i++) {
        inb(ATA_CONTROL);
    }
}

/* "Read the Regular Status port until bit 7 (BSY, value = 0x80)
 * clears, and bit 3 (DRQ, value = 8) sets -- or until bit 0 (ERR,
 * value = 1) or bit 5 (DF, value = 0x20) sets" (OSDev Wiki, "ATA PIO
 * Mode"). Returns 1 once the drive is genuinely ready to transfer
 * data, 0 if it reported a real error instead. */
static int ata_poll_ready(void) {
    uint8_t status;
    do {
        status = inb(ATA_STATUS);
    } while (status & ATA_SR_BSY);

    if (status & (ATA_SR_ERR | ATA_SR_DF)) {
        return 0;
    }
    return (status & ATA_SR_DRQ) != 0u;
}

int ata_identify(void) {
    /* "select a target drive by sending 0xA0 for the master drive...
     * to the 'drive select' IO port" (OSDev Wiki, "ATA PIO Mode") --
     * this driver only ever speaks to the primary master. */
    outb(ATA_DRIVE_SELECT, 0xA0u);
    ata_delay_400ns();

    outb(ATA_SECCOUNT, 0);
    outb(ATA_LBA_LO, 0);
    outb(ATA_LBA_MID, 0);
    outb(ATA_LBA_HI, 0);

    /* "Then send the IDENTIFY command (0xEC) to the Command IO port
     * (0x1F7). Then read the Status port (0x1F7) again. If the value
     * read is 0, the drive does not exist." */
    outb(ATA_COMMAND, ATA_CMD_IDENTIFY);
    uint8_t status = inb(ATA_STATUS);
    if (status == 0u) {
        kprintf("ata_identify: Status read back 0 -- no drive on the primary bus's master position\n");
        return 0;
    }

    /* "continue polling one of the Status ports until bit 3 (DRQ,
     * value = 8) sets, or until bit 0 (ERR, value = 1) sets." */
    while (1) {
        status = inb(ATA_STATUS);
        if (status & ATA_SR_ERR) {
            kprintf("ata_identify: ERR set while polling -- treating as no usable drive\n");
            return 0;
        }
        if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) {
            break;
        }
    }

    /* "At that point, if ERR is clear, the data is ready to read from
     * the Data port (0x1F0). Read 256 16-bit values, and store them."
     * This driver reads and discards all 256 -- the real IDENTIFY
     * data this chapter's own read/write demo does not need -- purely
     * so the drive's own internal state is left clean: leaving any of
     * those 256 words unread would leave DRQ set going into this
     * driver's very next command. */
    for (int i = 0; i < 256; i++) {
        (void) inw(ATA_DATA);
    }

    kprintf("ata_identify: real drive found on the primary bus's master position\n");
    return 1;
}

void ata_read_sector(uint32_t lba, uint8_t *buffer) {
    /* "Send 0xE0 for the 'master'... ORed with the highest 4 bits of
     * the LBA to port 0x1F6" (OSDev Wiki, "ATA PIO Mode") -- 28-bit
     * LBA addressing, so only bits [27:24] of `lba` ever reach this
     * register. */
    outb(ATA_DRIVE_SELECT, (uint8_t) (0xE0u | ((lba >> 24) & 0x0Fu)));
    ata_delay_400ns();

    outb(ATA_SECCOUNT, 1u);
    outb(ATA_LBA_LO, (uint8_t) (lba & 0xFFu));
    outb(ATA_LBA_MID, (uint8_t) ((lba >> 8) & 0xFFu));
    outb(ATA_LBA_HI, (uint8_t) ((lba >> 16) & 0xFFu));
    outb(ATA_COMMAND, ATA_CMD_READ_SECTORS);

    if (!ata_poll_ready()) {
        kprintf("ata_read_sector: LBA %u -- drive reported ERR/DF, no data transferred\n", lba);
        return;
    }

    /* "Transfer 256 16-bit values, a uint16_t at a time, into your
     * buffer from I/O port 0x1F0" -- one 512-byte sector is exactly
     * 256 real 16-bit words. */
    uint16_t *words = (uint16_t *) buffer;
    for (int i = 0; i < 256; i++) {
        words[i] = inw(ATA_DATA);
    }
}

void ata_write_sector(uint32_t lba, const uint8_t *buffer) {
    outb(ATA_DRIVE_SELECT, (uint8_t) (0xE0u | ((lba >> 24) & 0x0Fu)));
    ata_delay_400ns();

    outb(ATA_SECCOUNT, 1u);
    outb(ATA_LBA_LO, (uint8_t) (lba & 0xFFu));
    outb(ATA_LBA_MID, (uint8_t) ((lba >> 8) & 0xFFu));
    outb(ATA_LBA_HI, (uint8_t) ((lba >> 16) & 0xFFu));

    /* "To write sectors in 28 bit PIO mode, send command 'WRITE
     * SECTORS' (0x30) to the Command port." */
    outb(ATA_COMMAND, ATA_CMD_WRITE_SECTORS);

    if (!ata_poll_ready()) {
        kprintf("ata_write_sector: LBA %u -- drive reported ERR/DF, no data transferred\n", lba);
        return;
    }

    /* "Do not use REP OUTSW to transfer data. There must be a tiny
     * delay between each OUTSW output uint16_t." This loop already
     * issues one real `outw` per iteration rather than a single
     * REP-prefixed string instruction; the alternate-status read
     * between each word is this driver's own real, explicit delay,
     * reusing the exact same "read a status port and discard it"
     * technique ata_delay_400ns() already relies on above. */
    const uint16_t *words = (const uint16_t *) buffer;
    for (int i = 0; i < 256; i++) {
        outw(ATA_DATA, words[i]);
        inb(ATA_CONTROL);
    }

    /* "Make sure to do a Cache Flush (ATA command 0xE7) after each
     * write command completes" -- without this, a byte this driver
     * just wrote could still be sitting only in the emulated drive's
     * own write cache, never reaching the underlying disk image file
     * QEMU backs it with at all. */
    outb(ATA_COMMAND, ATA_CMD_CACHE_FLUSH);
    while (inb(ATA_STATUS) & ATA_SR_BSY) {
        /* wait for the flush itself to complete */
    }
}
