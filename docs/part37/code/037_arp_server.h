#ifndef UNIX_OS_037_ARP_SERVER_H
#define UNIX_OS_037_ARP_SERVER_H

#include <stdint.h>

/* Real ARP SERVER behavior: this kernel answering an incoming request
 * about its OWN address, the one real gap every earlier ARP chapter
 * (28, 29) named explicitly as out of scope -- 037_arp.h's own
 * top-of-file comment states it plainly: "no real ARP SERVER behavior
 * -- this kernel never answers an incoming ARP request asking about
 * its own address, only ever asks about someone else's."
 *
 * Cited directly from RFC 826's own full "Packet Reception" algorithm
 * (https://www.rfc-editor.org/rfc/rfc826.html), the second half Chapter
 * 29 deliberately left unimplemented (Chapter 29 only ever implemented
 * the Merge_flag/table-update half). The real branch this chapter
 * implements, quoted directly (re-fetched to confirm exact wording):
 *
 *   "?Am I the target protocol address? Yes: ... ?Is the opcode
 *   ares_op$REQUEST? (NOW look at the opcode!!) Yes: Swap hardware and
 *   protocol fields, putting the local hardware and protocol addresses
 *   in the sender fields. Set the ar$op field to ares_op$REPLY. Send
 *   the packet to the (new) target hardware address on the same
 *   hardware on which the request was received."
 *
 * Real, honest scope note: RFC 826's own quoted branch above couples
 * the reply step with a Merge_flag table-update step ("If Merge_flag
 * is false, add the triplet ... to the translation table") -- this
 * chapter deliberately implements only the reply half, not the merge
 * half. Feeding an incoming request's own sender information into
 * Chapter 29's real ARP cache (arp_cache_insert()) would be a genuine,
 * real behavior most production ARP implementations exhibit, but this
 * chapter's own stated scope is reply-only, kept deliberately separate
 * from Chapter 29's cache rather than silently coupling the two.
 *
 * This kernel only ever speaks Ethernet/IPv4 (037_arp.h's own
 * ARP_HTYPE_ETHERNET/ARP_PTYPE_IPV4), so "Do I have the hardware type
 * in ar$hrd? ... Do I speak the protocol in ar$pro?" -- RFC 826's own
 * opening two checks, already implemented once in 037_arp.c's own
 * arp_receive_reply() -- are repeated here rather than shared, since
 * this chapter's own arp_server_handle_frame() takes a raw received
 * frame directly (a real incoming request, not a reply this kernel is
 * itself waiting on) and needs its own independent parse. */

/* Examines one real received frame. If it is a real ARP request (RFC
 * 826 ar$op == REQUEST) whose own target protocol address (ar$tpa)
 * matches `my_ip`, builds and sends a real ARP reply -- per RFC 826's
 * own quoted branch above -- to the real requester's own hardware
 * address, and returns 1. Otherwise (not ARP at all, not Ethernet/
 * IPv4, not a REQUEST, or a REQUEST for a different real target IP)
 * does nothing and returns 0 -- this kernel's own real refusal to
 * answer on behalf of an address that is not its own, never a
 * partial or best-effort reply. */
int arp_server_handle_frame(const uint8_t *rx_frame, uint32_t rx_len,
                             const uint8_t *my_mac, const uint8_t my_ip[4]);

#endif
