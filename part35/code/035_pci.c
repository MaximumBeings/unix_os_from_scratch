/* This chapter's own new driver: real PCI bus enumeration, cited
 * field-for-field from OSDev Wiki's own "PCI" page
 * (https://wiki.osdev.org/PCI) -- see 035_pci.h's own top-of-file
 * comment for the full citation of CONFIG_ADDRESS/CONFIG_DATA and the
 * real bit layout this file builds by hand below.
 *
 * Every driver this book has written before this chapter has talked
 * to hardware through ordinary 8-bit or 16-bit port I/O (outb/inb,
 * outw). This chapter's own pci_config_read_dword() is the first
 * place this kernel ever performs 32-bit port I/O (outl/inl) --
 * CONFIG_DATA hands back one full 32-bit configuration-space dword
 * per real access, regardless of how narrow the field this driver
 * actually wants is, so every individual field below (a 16-bit vendor
 * ID, an 8-bit class code, a single header-type byte) is extracted by
 * shifting and masking that dword in software, not by a narrower port
 * read.
 *
 * New this chapter: pci_config_write_dword(), the write half of the
 * same mechanism, factored through a new shared pci_config_address()
 * helper both functions now call -- needed because this chapter's own
 * new 035_rtl8139.h/.c must WRITE the PCI Command register before the
 * device will respond to I/O-port accesses or DMA at all. */

#include <stdint.h>

#include "035_pci.h"
#include "035_printf.h"

#define PCI_CONFIG_ADDRESS 0xCF8u
#define PCI_CONFIG_DATA    0xCFCu

static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* Builds a real CONFIG_ADDRESS value field-for-field, per the bit
 * layout cited in 035_pci.h: bit 31 is the Enable bit (always set
 * here -- this driver never reads CONFIG_DATA without it), bits
 * 23-16 the bus number, bits 15-11 the device number (masked to 5
 * real bits, 0-31), bits 10-8 the function number (masked to 3 real
 * bits, 0-7), and bits 7-0 the register offset, with its own low two
 * bits forced to 0 exactly as the cited page requires ("the two
 * lowest bits of CONFIG_ADDRESS must always be zero"). */
static uint32_t pci_config_address(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    return 0x80000000u
        | ((uint32_t) bus << 16)
        | (((uint32_t) device & 0x1Fu) << 11)
        | (((uint32_t) function & 0x07u) << 8)
        | ((uint32_t) offset & 0xFCu);
}

uint32_t pci_config_read_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    outl(PCI_CONFIG_ADDRESS, pci_config_address(bus, device, function, offset));
    return inl(PCI_CONFIG_DATA);
}

/* This chapter's own new write half of Configuration Mechanism #1,
 * cited directly in 035_pci.h's own top-of-file comment -- builds the
 * exact same real CONFIG_ADDRESS value pci_config_read_dword() does,
 * via the same pci_config_address() helper, then writes `value` to
 * CONFIG_DATA instead of reading it. Used by 035_rtl8139.c to enable
 * the real "I/O Space" and "Bus Master" bits in the PCI Command
 * register before this chapter's own new network driver touches the
 * device at all. */
void pci_config_write_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value) {
    outl(PCI_CONFIG_ADDRESS, pci_config_address(bus, device, function, offset));
    outl(PCI_CONFIG_DATA, value);
}

/* One real function's worth of configuration space, decoded from
 * exactly three real dword reads (offsets 0x00, 0x08, 0x0C). Returns
 * 0 immediately, touching nothing in `out`, the instant the Vendor ID
 * field reads back 0xFFFF -- "Since there are no vendors that ==
 * 0xFFFF, it must be a non-existent device", cited directly in
 * 035_pci.h. Every other field decoded below follows the same real,
 * standard PCI configuration-space layout: offset 0x00 packs Device
 * ID (bits 31-16) over Vendor ID (bits 15-0); offset 0x08 packs Class
 * Code (bits 31-24), Subclass (bits 23-16), Prog IF (bits 15-8), and
 * Revision ID (bits 7-0); offset 0x0C packs BIST (bits 31-24), Header
 * Type (bits 23-16), Latency Timer (bits 15-8), and Cache Line Size
 * (bits 7-0) -- this driver only ever needs the first three of those
 * four fields. */
static int pci_probe_function(uint8_t bus, uint8_t device, uint8_t function,
                               struct pci_device *out) {
    uint32_t dword00 = pci_config_read_dword(bus, device, function, 0x00);
    uint16_t vendor_id = (uint16_t) (dword00 & 0xFFFFu);
    if (vendor_id == PCI_VENDOR_NONE) {
        return 0;
    }

    uint32_t dword08 = pci_config_read_dword(bus, device, function, 0x08);
    uint32_t dword0c = pci_config_read_dword(bus, device, function, 0x0C);

    out->bus = bus;
    out->device = device;
    out->function = function;
    out->vendor_id = vendor_id;
    out->device_id = (uint16_t) ((dword00 >> 16) & 0xFFFFu);
    out->revision_id = (uint8_t) (dword08 & 0xFFu);
    out->prog_if = (uint8_t) ((dword08 >> 8) & 0xFFu);
    out->subclass = (uint8_t) ((dword08 >> 16) & 0xFFu);
    out->class_code = (uint8_t) ((dword08 >> 24) & 0xFFu);
    out->header_type = (uint8_t) ((dword0c >> 16) & 0xFFu);
    return 1;
}

/* Probes function 0 of a real (bus, device) pair, and every other
 * real function 1-7 too when function 0's own Header Type byte sets
 * the Multi-Function bit -- cited directly in 035_pci.h: "If it's not
 * a multi-function device, then there is only one PCI host
 * controller... If it's a multi-function device... check remaining
 * functions." Calls `visit` once per real device function found;
 * returns nothing, since this is shared by both pci_enumerate() (which
 * wants every match) and pci_find_by_class() (which wants only the
 * first). */
static void pci_probe_device(uint16_t bus, uint8_t device,
                              void (*visit)(const struct pci_device *, void *), void *ctx) {
    struct pci_device info;
    if (!pci_probe_function((uint8_t) bus, device, 0, &info)) {
        return;
    }
    visit(&info, ctx);

    if (info.header_type & PCI_HEADER_TYPE_MULTIFUNCTION_BIT) {
        for (uint8_t function = 1; function < 8; function++) {
            struct pci_device finfo;
            if (pci_probe_function((uint8_t) bus, device, function, &finfo)) {
                visit(&finfo, ctx);
            }
        }
    }
}

static void enumerate_visit(const struct pci_device *info, void *ctx) {
    int *count = (int *) ctx;
    (*count)++;
    kprintf("  %u:%u.%u  vendor=%x device=%x class=%x subclass=%x header=%x\n",
            (unsigned) info->bus, (unsigned) info->device, (unsigned) info->function,
            (unsigned) info->vendor_id, (unsigned) info->device_id,
            (unsigned) info->class_code, (unsigned) info->subclass,
            (unsigned) (info->header_type & PCI_HEADER_TYPE_MASK));
}

/* A real brute-force scan of every possible (bus, device) pair --
 * "For the brute force method... there are 32 devices per bus and 256
 * buses, so you call 'checkDevice()' 8192 times", cited directly in
 * 035_pci.h. This driver never assumes which buses actually exist the
 * way a recursive, bridge-walking scan would; it simply asks all
 * 8192 real (bus, device) coordinates, function 0 first, and trusts
 * the real Vendor ID = 0xFFFF sentinel to skip everything empty. */
void pci_enumerate(void) {
    kprintf("PCI: brute-force scan of 256 buses x 32 devices...\n");
    int found_count = 0;

    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t device = 0; device < 32; device++) {
            pci_probe_device(bus, device, enumerate_visit, &found_count);
        }
    }

    kprintf("PCI: scan complete, %d real device function(s) found.\n", found_count);
}

struct find_by_class_ctx {
    uint8_t class_code;
    uint8_t subclass;
    struct pci_device *out;
    int found;
};

static void find_by_class_visit(const struct pci_device *info, void *ctx_ptr) {
    struct find_by_class_ctx *ctx = (struct find_by_class_ctx *) ctx_ptr;
    if (ctx->found) {
        return;
    }
    if (info->class_code == ctx->class_code && info->subclass == ctx->subclass) {
        *ctx->out = *info;
        ctx->found = 1;
    }
}

/* The same real brute-force scan pci_enumerate() performs, stopping
 * the instant a real device function's own class code and subclass
 * match. This driver still probes every function of a matching
 * device's own (bus, device) pair before deciding there's no match at
 * a given coordinate (pci_probe_device() above always visits every
 * real function it finds), but the outer bus/device loop itself stops
 * as soon as `ctx.found` is set, so a match early in the scan is
 * genuinely cheap, not merely early-printed. */
int pci_find_by_class(uint8_t class_code, uint8_t subclass, struct pci_device *out) {
    struct find_by_class_ctx ctx;
    ctx.class_code = class_code;
    ctx.subclass = subclass;
    ctx.out = out;
    ctx.found = 0;

    for (uint16_t bus = 0; bus < 256 && !ctx.found; bus++) {
        for (uint8_t device = 0; device < 32 && !ctx.found; device++) {
            pci_probe_device(bus, device, find_by_class_visit, &ctx);
        }
    }

    return ctx.found;
}
