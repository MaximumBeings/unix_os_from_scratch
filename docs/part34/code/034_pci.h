#ifndef UNIX_OS_034_PCI_H
#define UNIX_OS_034_PCI_H

#include <stdint.h>

/* This chapter's first driver that never touches a fixed, hardcoded
 * I/O port range the way every earlier driver has: Chapter 19's own
 * ATA driver always talks to 0x1F0-0x1F7 because the PC platform's
 * own legacy convention says the primary IDE controller lives there,
 * full stop -- no lookup involved. A real network card has no such
 * fixed address. Its own I/O base, memory base, and IRQ line are
 * assigned by firmware at boot and can differ from one real machine
 * to the next; the only fixed thing about it is the two 32-bit ports
 * this chapter's own code talks to in order to ASK the hardware where
 * everything else lives: CONFIG_ADDRESS (0xCF8) and CONFIG_DATA
 * (0xCFC), cited field-for-field from OSDev Wiki's own "PCI" page
 * (https://wiki.osdev.org/PCI). "CONFIG_ADDRESS specifies the
 * configuration address that is required to be accesses, while
 * accesses to CONFIG_DATA will actually generate the configuration
 * access." Writing a 32-bit value built from a bus/device/function/
 * register-offset to CONFIG_ADDRESS, then reading (or writing)
 * CONFIG_DATA, is "Configuration Mechanism #1" -- the original,
 * universally supported PCI configuration access method, and the one
 * this chapter's driver uses.
 *
 * The CONFIG_ADDRESS register's own real bit layout, cited directly
 * from the same page:
 *
 *   Bit 31      Enable bit (must be 1 for CONFIG_DATA to do anything)
 *   Bits 30-24  Reserved (must be 0)
 *   Bits 23-16  Bus Number    (0-255)
 *   Bits 15-11  Device Number (0-31)
 *   Bits 10-8   Function Number (0-7)
 *   Bits 7-0    Register Offset (the low two bits are always 0 --
 *               "the two lowest bits of CONFIG_ADDRESS must always be
 *               zero, with the remaining six bits allowing you to
 *               choose each of the 64 32-bit words" of a device's own
 *               256-byte configuration space)
 *
 * A device that does not exist reads back as all-ones on its Vendor
 * ID field: "Since there are no vendors that == 0xFFFF, it must be a
 * non-existent device." -- exactly how this driver tells a real,
 * present device apart from an empty device slot, below. */

/* PCI class codes this chapter's own code looks for by name, cited
 * from OSDev Wiki's own "PCI" page, "Class Codes" table: */
#define PCI_CLASS_MASS_STORAGE   0x01u  /* "0x1 - Mass Storage Controller" */
#define PCI_CLASS_NETWORK        0x02u  /* "0x2 - Network Controller" */
#define PCI_CLASS_BRIDGE         0x06u  /* "0x6 - Bridge" */

#define PCI_SUBCLASS_IDE         0x01u  /* "0x1 - IDE Controller" */
#define PCI_SUBCLASS_ETHERNET    0x00u  /* "0x0 - Ethernet Controller" */
#define PCI_SUBCLASS_PCI_BRIDGE  0x04u  /* "0x4 - PCI-to-PCI Bridge" */

/* The Header Type byte (configuration space offset 0x0E) packs two
 * separate things into one byte, cited from the same page: bits 6-0
 * are the real header type value (0x00 for an ordinary device, 0x01
 * for a PCI-to-PCI bridge), and bit 7 alone is the Multi-Function
 * flag -- "If it's not a multi-function device, then there is only
 * one PCI host controller [i.e. only function 0 is real]... If it's a
 * multi-function device... check remaining functions." */
#define PCI_HEADER_TYPE_MASK              0x7Fu
#define PCI_HEADER_TYPE_MULTIFUNCTION_BIT 0x80u

/* "Since there are no vendors that == 0xFFFF, it must be a
 * non-existent device" -- the exact sentinel this driver checks for
 * every function it probes. */
#define PCI_VENDOR_NONE 0xFFFFu

/* Everything this chapter's own code learns about one real PCI
 * function, decoded out of the three real 32-bit configuration-space
 * reads pci_probe_function() below actually performs. Deliberately
 * narrow: only the fields this chapter's own enumeration and
 * class-code lookup need, not the full 256-byte configuration space
 * (BARs -- Base Address Registers, which is where a real driver would
 * next learn a device's own I/O or memory base -- are left for the
 * chapter that writes a real driver against one of these devices). */
struct pci_device {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t revision_id;
    uint8_t header_type;
};

/* One real 32-bit configuration-space read, at real bus/device/
 * function/register-offset coordinates. `offset` is masked to a
 * dword boundary internally (bits 1-0 forced to 0), exactly as
 * CONFIG_ADDRESS itself requires -- every field this driver ever
 * decodes comes from shifting and masking the dword this returns, in
 * software, never from a narrower port read, since CONFIG_DATA only
 * ever hands back a full 32 bits at a time regardless of how small
 * the field you actually want is. */
uint32_t pci_config_read_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);

/* A real brute-force scan of every possible (bus, device) pair --
 * "For the brute force method... there are 32 devices per bus and 256
 * buses" -- checking function 0 of each, and every other function
 * too when that device's own Header Type byte sets the
 * Multi-Function bit. Prints one real line per real device function
 * found (bus:device.function, vendor ID, device ID, class, subclass,
 * header type) via kprintf, and a final count. */
void pci_enumerate(void);

/* The brute-force scan again, but stopping at (and returning, via
 * `out`) the first real device function whose own class code and
 * subclass match. Returns 1 and fills `out` if such a device was
 * found anywhere across all 256 buses; returns 0, leaving `out`
 * untouched, if no real device on this machine matches. This is the
 * real entry point a future chapter's own network or storage driver
 * would call first, to learn a specific device's real bus/device/
 * function coordinates before reading anything from its own BARs. */
int pci_find_by_class(uint8_t class_code, uint8_t subclass, struct pci_device *out);

/* New this chapter: the write half of Configuration Mechanism #1.
 * Every earlier chapter's own use of this file only ever READ
 * configuration space -- this chapter's own new network driver
 * (034_rtl8139.h/.c) needs to WRITE the real PCI Command register
 * (configuration-space offset 0x04) before the device will do
 * anything at all: bit 0 ("I/O Space") and bit 2 ("Bus Master"),
 * cited directly from OSDev Wiki's own "PCI" page's own Command
 * Register bit table -- "If set to 1 the device can respond to I/O
 * Space accesses" and "If set to 1 the device can behave as a bus
 * master; otherwise, the device can not generate PCI accesses."
 * Builds the exact same real CONFIG_ADDRESS value
 * pci_config_read_dword() does, then writes `value` to CONFIG_DATA
 * instead of reading it. */
void pci_config_write_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value);

#endif
