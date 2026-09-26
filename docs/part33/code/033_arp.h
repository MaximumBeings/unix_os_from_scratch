#ifndef UNIX_OS_033_ARP_H
#define UNIX_OS_033_ARP_H

#include <stdint.h>

/* Chapter 28's own new, minimal ARP client, built entirely on top of
 * 033_rtl8139.c's own real rtl8139_send()/rtl8139_receive_next_packet()
 * -- no new hardware, no new driver, just the next real protocol layer
 * above the raw Ethernet frames Chapters 25-27 already send and
 * receive. Cited field-for-field from two real sources: RFC 826 ("An
 * Ethernet Address Resolution Protocol", the original 1982 standard,
 * https://www.rfc-editor.org/rfc/rfc826.html) for the real packet
 * field names/order ("ar$hrd", "ar$pro", "ar$hln", "ar$pln", "ar$op",
 * "ar$sha", "ar$spa", "ar$tha", "ar$tpa"), the real opcode values
 * ("REQUEST = 1", "REPLY = 2"), and the real requirement that a
 * request be broadcast ("It then causes this packet to be broadcast
 * to all stations on the Ethernet cable"); and OSDev Wiki's own
 * "Address Resolution Protocol" page
 * (https://wiki.osdev.org/Address_Resolution_Protocol) for the same
 * fields laid out as a real, compilable C struct (htype/ptype/hlen/
 * plen/opcode/srchw/srcpr/dsthw/dstpr, htype "Ethernet is 0x1", ptype
 * "IP is 0x0800"), and its own real, practical detail that most
 * implementations zero the ARP payload's own target hardware address
 * field on a request, since it isn't known yet -- that is the whole
 * point of asking.
 *
 * One real gap neither of those two sources fills, that this chapter
 * had to cite from a third, authoritative primary source: neither
 * ever states which real EtherType value belongs in the ETHERNET
 * frame's own header when its payload is an ARP packet (RFC 826
 * predates the very idea of an EtherType field; OSDev's own ARP page
 * only ever discusses the ARP payload's own internal ptype field, IP's
 * own 0x0800, never the frame-level value). Cited instead directly
 * from IANA's own official IEEE 802 Numbers registry
 * (https://www.iana.org/assignments/ieee-802-numbers/ieee-802-numbers.txt,
 * assignment authority RFC 9542): "2054  0806  ...  Address
 * Resolution Protocol (ARP)" -- EtherType 0x0806.
 *
 * This chapter's own demo resolves one real, fixed IP address: QEMU's
 * own real default gateway under user-mode ("SLIRP") networking,
 * cited directly from QEMU's own official documentation
 * (https://www.qemu.org/docs/master/system/devices/net.html), whose
 * own network diagram shows "Firewall/DHCP server <-----> Internet
 * (10.0.2.2)" -- a real host genuinely reachable over this exact QEMU
 * command line's own -netdev user backend, not a value this chapter
 * invented or assumed. Resolving it for real requires this chapter's
 * own new, real, non-loopback mode (see 033_rtl8139.c's own
 * rtl8139_init() and its new `enable_loopback` parameter): Chapters
 * 25-27's own real hardware loopback mode routes every transmitted
 * frame straight back to this device's own receiver and never puts a
 * single real bit on the wire, so a real reply from a real (if
 * QEMU-emulated) host on the other end could never arrive that way.
 *
 * Deliberately out of this chapter's own stated scope, per RFC 826's
 * own full "Packet Reception" algorithm (which this chapter's own
 * arp_receive_reply() only implements the relevant slice of, not the
 * whole thing): no ARP translation-table cache (this driver resolves
 * one address, once, per call, and keeps nothing), no gratuitous-ARP
 * announcement, and no real ARP SERVER behavior -- this kernel never
 * answers an incoming ARP request asking about its own address, only
 * ever asks about someone else's. */

#define ARP_HTYPE_ETHERNET 1u      /* RFC 826 ar$hrd; OSDev: "Ethernet is 0x1" */
#define ARP_PTYPE_IPV4     0x0800u /* RFC 826 ar$pro; OSDev: "IP is 0x0800" */
#define ARP_HLEN_ETHERNET  6u      /* RFC 826 ar$hln: a real 6-byte MAC */
#define ARP_PLEN_IPV4      4u      /* RFC 826 ar$pln: a real 4-byte IPv4 address */
#define ARP_OP_REQUEST     1u      /* RFC 826 ar$op: "REQUEST = 1" */
#define ARP_OP_REPLY       2u      /* RFC 826 ar$op: "REPLY = 2" */

/* Cited directly, IANA's own IEEE 802 Numbers registry: the real
 * Ethernet-frame-level EtherType for an ARP payload. */
#define ETHERTYPE_ARP 0x0806u

/* A real ARP packet's own fields, already converted to this machine's
 * own native byte order by arp_receive_reply() below -- NEVER laid
 * directly over real wire bytes (the real wire format's own 16-bit
 * fields are big-endian; this kernel's own x86 registers are
 * little-endian, exactly the same real reason 033_kmain.c's own
 * build_demo_frame() has always written its EtherType as two separate
 * bytes rather than one 16-bit store). RFC 826's own field names in
 * comments; this book's own snake_case in code. */
typedef struct {
    uint16_t htype;          /* ar$hrd */
    uint16_t ptype;          /* ar$pro */
    uint8_t  hlen;            /* ar$hln */
    uint8_t  plen;             /* ar$pln */
    uint16_t opcode;          /* ar$op */
    uint8_t  sender_mac[6];   /* ar$sha */
    uint8_t  sender_ip[4];    /* ar$spa */
    uint8_t  target_mac[6];   /* ar$tha */
    uint8_t  target_ip[4];    /* ar$tpa */
} arp_packet_t;

/* This chapter's own real 60-byte frame: a real 14-byte Ethernet
 * header plus a real 28-byte ARP payload (2+2+1+1+2+6+4+6+4 = 28
 * bytes, 42 total), padded to the real IEEE 802.3 minimum this book
 * has cited since Chapter 25 (see 033_kmain.c's own DEMO_FRAME_SIZE
 * comment). */
#define ARP_FRAME_SIZE 60u

/* Builds and sends one real ARP request: Ethernet destination
 * broadcast (ff:ff:ff:ff:ff:ff, cited directly -- RFC 826: the
 * request "is ... broadcast to all stations on the Ethernet cable"),
 * ARP payload's own target hardware address zeroed (cited directly --
 * OSDev: "For an ARP request most implementations zero the
 * destination MAC address"), everything else cited field-for-field
 * above. `src_mac`/`src_ip` are this machine's own real, already-known
 * values; `target_ip` is the real IPv4 address being resolved.
 * Returns 1 on success, 0 on real refusal (this chapter's own request
 * is always exactly ARP_FRAME_SIZE bytes, so the only way this could
 * happen is exceeding RTL8139_MAX_FRAME, which can never happen
 * here).
 *
 * This chapter's own real fix, changed from Chapter 28: sends via
 * 033_rtl8139.c's own rtl8139_send_queue() (real round-robin
 * descriptor allocation) plus rtl8139_wait_descriptor_sent() on
 * whichever real descriptor it used, NOT the single-descriptor
 * rtl8139_send() Chapter 28 used. Chapter 28 only ever sent one real
 * ARP request per boot, so it never hit Chapter 27's own real,
 * already-documented hazard: retriggering the SAME descriptor twice
 * in a row, with no other descriptor's own transmission in between,
 * can leave that second real send genuinely stuck (033_rtl8139.c's
 * own rtl8139_send() comment). This chapter's own cache demo
 * genuinely sends several fresh real ARP requests in one boot,
 * hitting that exact hazard the first time this driver ever called
 * rtl8139_send() twice -- fixed by using the same real round-robin
 * workaround Chapter 27 already established for its own repeated
 * sends. */
int arp_send_request(const uint8_t *src_mac, const uint8_t src_ip[4],
                      const uint8_t target_ip[4]);

/* Blocks (via 033_rtl8139.c's own real, interrupt-driven
 * rtl8139_receive_next_packet(), called up to `max_attempts` times)
 * until a real frame arrives whose Ethernet header's own EtherType is
 * ETHERTYPE_ARP, whose ARP payload's own hardware/protocol type and
 * length fields match this driver's only real case (Ethernet/IPv4 --
 * the same "do I speak this hardware type/protocol" check RFC 826's
 * own reception algorithm opens with), whose opcode is ARP_OP_REPLY,
 * and whose own sender protocol address matches `expected_sender_ip`
 * -- filtering out any other real traffic this exact QEMU network
 * segment might genuinely deliver (this chapter's own demo is the
 * first in this book where the device is not in loopback mode, so
 * real frames this kernel never sent can genuinely arrive and must be
 * told apart from the one real reply being waited for). Copies the
 * matching real ARP payload, with its own multi-byte fields already
 * converted to this machine's own native byte order, into
 * `*out_reply` and returns 1 on success, or returns 0 if
 * `max_attempts` real packets were read and consumed without a match
 * -- an honest, bounded failure rather than an infinite real `hlt`
 * wait. */
int arp_receive_reply(uint32_t max_attempts, const uint8_t expected_sender_ip[4],
                       arp_packet_t *out_reply);

#endif
