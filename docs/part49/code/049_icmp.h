#ifndef UNIX_OS_049_ICMP_H
#define UNIX_OS_049_ICMP_H

#include <stdint.h>

/* A real ICMPv4 Echo Request/Reply codec (the real "ping" message
 * pair, RFC 792) on top of 049_ip.h's own real IPv4 header, built
 * entirely on 049_rtl8139.c's own real, already-established
 * rtl8139_send_queue()/rtl8139_receive_next_packet() -- the same
 * layering 049_arp.c already established for ARP: this file's own
 * send/receive functions handle the full real Ethernet+IP+ICMP frame
 * themselves, mirroring arp_send_request()/arp_receive_reply()'s own
 * shape.
 *
 * RFC 792's own official text is blocked by this sandbox's network
 * egress policy, the same pattern 049_ip.h's own top-of-file comment
 * already documents for RFC 791. This chapter cloned the same real,
 * widely-deployed open-source embedded TCP/IP stack
 * (github.com/lwip-tcpip/lwip) and read its own real
 * `src/include/lwip/prot/icmp.h` in full. Every real value and field
 * name below is copied directly out of that real file's own real
 * `ICMP_ECHO`/`ICMP_ER` type constants and its own real
 * `struct icmp_echo_hdr`:
 *
 *   byte 0:      type (8 = Echo Request, 0 = Echo Reply -- cited
 *                directly from lwIP's own real `ICMP_ECHO`/`ICMP_ER`)
 *   byte 1:      code (always 0 for echo)
 *   bytes 2-3:   checksum (the same real RFC 1071 algorithm 049_ip.h
 *                already cites and implements -- ICMP's own checksum
 *                is not a distinct algorithm, so 049_icmp.c reuses
 *                049_ip.c's own `ip4_checksum()` rather than
 *                reimplementing it)
 *   bytes 4-5:   identifier
 *   bytes 6-7:   sequence number
 *   remaining:   payload -- RFC 792's own real requirement that an
 *                Echo Reply copy the Echo Request's own data back
 *                verbatim is this file's own `icmp_receive_echo_reply()`
 *                check, confirmed against whatever payload
 *                `icmp_send_echo_request()` actually sent.
 *
 * The real Ethernet-frame-level EtherType for an IPv4 payload --
 * 0x0800 -- is cited the same way 049_arp.h's own top-of-file comment
 * already cites ARP's EtherType: IANA's own official IEEE 802 Numbers
 * registry (https://www.iana.org/assignments/ieee-802-numbers/ieee-802-numbers.txt,
 * assignment authority RFC 9542), and independently confirmed by
 * 049_arp.h's own real ARP payload `ptype` field citation ("IP is
 * 0x0800" -- the same real value, since an ARP packet's own internal
 * protocol-type field and an Ethernet frame's own EtherType both
 * identify "this carries IPv4" using the identical real registered
 * number). */

#define ICMP_ECHO_HEADER_LEN 8u
#define ICMP_ECHO_PAYLOAD_MAX 32u
#define ICMP_TYPE_ECHO_REQUEST 8u /* cited from lwIP's own real ICMP_ECHO */
#define ICMP_TYPE_ECHO_REPLY 0u   /* cited from lwIP's own real ICMP_ER */

#define ETHERTYPE_IPV4 0x0800u

typedef struct {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t identifier;
    uint16_t sequence;
    uint8_t payload[ICMP_ECHO_PAYLOAD_MAX];
    uint32_t payload_len;
} icmp_echo_t;

/* Builds a real ICMP Echo header+payload into `out`. Computes and
 * fills in the real checksum itself. Returns the encoded length
 * (ICMP_ECHO_HEADER_LEN + echo->payload_len), or 0 on refusal if
 * `out_size` is too small or `echo->payload_len` exceeds
 * ICMP_ECHO_PAYLOAD_MAX. */
uint32_t icmp_build_echo(const icmp_echo_t *echo, uint8_t *out, uint32_t out_size);

/* Parses exactly `len` bytes of a real ICMP Echo header+payload back
 * into `*out`. Returns 1 on success, or 0 -- refusing outright -- if
 * `len` is too short, the payload would exceed ICMP_ECHO_PAYLOAD_MAX,
 * or the real checksum does not verify. Accepts either real `type`
 * (Echo Request or Echo Reply); the caller checks which. */
int icmp_parse_echo(const uint8_t *buf, uint32_t len, icmp_echo_t *out);

/* Builds and sends one real Ethernet+IPv4+ICMP frame carrying a real
 * Echo Request (type 8) to `dst_ip` via `dst_mac` (already resolved,
 * e.g. by 049_arp_cache.c's own arp_resolve()). `identifier`/
 * `sequence` are this chapter's own caller-chosen values (RFC 792
 * leaves their real use up to the sender, beyond requiring a matching
 * Echo Reply to carry them back unchanged). Returns 1 on success, or
 * 0 on refusal (`payload_len` exceeding ICMP_ECHO_PAYLOAD_MAX, or the
 * real rtl8139_send_queue() refusing). */
int icmp_send_echo_request(const uint8_t *src_mac, const uint8_t src_ip[4],
                            const uint8_t dst_mac[6], const uint8_t dst_ip[4],
                            uint16_t identifier, uint16_t sequence, const uint8_t *payload,
                            uint32_t payload_len);

/* Blocks (via 049_rtl8139.c's own real, interrupt-driven
 * rtl8139_receive_next_packet(), called up to `max_attempts` times)
 * until a real frame arrives whose Ethernet header's own EtherType is
 * ETHERTYPE_IPV4, whose real IPv4 header parses (049_ip.c's own
 * ip4_parse_header()) with `protocol == IP4_PROTO_ICMP` and
 * `src_addr == expected_src_ip`, and whose real ICMP payload parses
 * (icmp_parse_echo()) as a real Echo Reply (`type ==
 * ICMP_TYPE_ECHO_REPLY`) matching both `expected_identifier` and
 * `expected_sequence` -- mirroring 049_arp.h's own real
 * arp_receive_reply()'s own filtering shape exactly. Copies the
 * matching real Echo Reply into `*out_reply` and returns 1 on
 * success, or returns 0 if `max_attempts` real packets were read and
 * consumed without a match -- an honest, bounded failure, never an
 * infinite real `hlt` wait. */
int icmp_receive_echo_reply(uint32_t max_attempts, const uint8_t expected_src_ip[4],
                             uint16_t expected_identifier, uint16_t expected_sequence,
                             icmp_echo_t *out_reply);

#endif
