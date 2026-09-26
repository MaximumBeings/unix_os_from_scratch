#ifndef UNIX_OS_028_ATA_H
#define UNIX_OS_028_ATA_H

#include <stdint.h>

/* This chapter's own real disk: a PIO-mode ATA/IDE driver for the
 * primary bus's master drive, cited field-for-field from OSDev Wiki
 * ("ATA PIO Mode": https://wiki.osdev.org/ATA_PIO_Mode). "The first
 * two buses are called the Primary and Secondary ATA bus, and are
 * almost always controlled by IO ports 0x1F0 through 0x1F7, and 0x170
 * through 0x177, respectively." This driver only ever touches the
 * primary bus (0x1F0-0x1F7, plus the alternate status port at 0x3F6)
 * -- the same bus QEMU's own default PC machine hands this chapter's
 * own attached `-drive ...,if=ide,index=0` hard disk, with the GRUB
 * boot ISO itself living on the secondary bus instead (see the
 * chapter text for how that was confirmed, not merely assumed). */

/* Every 512-byte sector this driver ever reads or writes, in one real
 * round trip. */
#define ATA_SECTOR_SIZE 512u

/* One-time real hardware check: is a drive actually present on the
 * primary bus's master position at all? Sends the IDENTIFY command
 * (0xEC) and checks the Status port, cited directly (OSDev Wiki, "ATA
 * PIO Mode": "Then send the IDENTIFY command (0xEC) to the Command IO
 * port (0x1F7). Then read the Status port (0x1F7) again. If the value
 * read is 0, the drive does not exist."). Returns 1 if a drive
 * responded, 0 if the Status port read back exactly 0. Must be called
 * before ata_read_sector()/ata_write_sector() -- this driver never
 * assumes a drive is present without having checked for real. */
int ata_identify(void);

/* Reads exactly one real 512-byte sector at 28-bit LBA `lba` from the
 * primary master drive into `buffer` (must be at least
 * ATA_SECTOR_SIZE bytes), via ordinary PIO -- no IRQ, polling the
 * Status port directly, cited from the same page's own 28-bit LBA
 * read procedure. */
void ata_read_sector(uint32_t lba, uint8_t *buffer);

/* Writes exactly one real 512-byte sector from `buffer` to 28-bit LBA
 * `lba` on the primary master drive, via ordinary PIO, followed by a
 * real Cache Flush command (0xE7) -- cited directly ("Make sure to do
 * a Cache Flush (ATA command 0xE7) after each write command
 * completes"), so a later read (even after this kernel halts and
 * QEMU exits) reads back what was genuinely written to the underlying
 * disk image, not merely a value still sitting in the emulated
 * drive's own write cache. */
void ata_write_sector(uint32_t lba, const uint8_t *buffer);

#endif
