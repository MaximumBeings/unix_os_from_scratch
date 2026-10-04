#ifndef UNIX_OS_048_IP_H
#define UNIX_OS_048_IP_H

#include <stdint.h>

/* A real, minimal IPv4 header codec (RFC 791) -- this chapter's own
 * confirmed next step up the network stack after Chapters 27-28's
 * real Ethernet/ARP work.
 *
 * RFC 791's own official text (rfc-editor.org, ietf.org,
 * datatracker.ietf.org) is blocked by this sandbox's network egress
 * policy, the same pattern every blocked-standards-site chapter since
 * Chapter 33 has hit. This chapter instead cloned
 * github.com/lwip-tcpip/lwip -- lwIP, a real, widely-deployed
 * open-source embedded TCP/IP stack (used in real products across the
 * embedded-systems industry, not a toy or teaching example) -- and
 * read its own real `src/include/lwip/prot/ip4.h` in full: a real,
 * complete, actively-maintained implementation of exactly this real
 * header, not a search summary. Every field name and bit layout below
 * is copied directly out of that real file's own real `struct ip_hdr`
 * and its own real `IP_RF`/`IP_DF`/`IP_MF`/`IP_OFFMASK` bit-mask
 * constants:
 *
 *   byte 0:      version (high nibble) / header length in 32-bit
 *                words (low nibble, real IHL field)
 *   byte 1:      type of service
 *   bytes 2-3:   total length
 *   bytes 4-5:   identification
 *   bytes 6-7:   flags (top 3 bits: reserved/DF/MF) + fragment
 *                offset (bottom 13 bits) -- one real combined 16-bit
 *                wire field, exactly as lwIP's own `struct ip_hdr`
 *                itself stores it (`_offset`), not two separate
 *                fields
 *   byte 8:      time to live
 *   byte 9:      protocol
 *   bytes 10-11: header checksum
 *   bytes 12-15: source address
 *   bytes 16-19: destination address
 *
 * The real Internet checksum algorithm itself (RFC 1071's own
 * one's-complement-sum-with-end-around-carry, then inverted) is cited
 * from that same cloned repository's own real
 * `src/core/inet_chksum.c`, `lwip_standard_chksum()` -- reimplemented
 * here from scratch (this file's own `ip4_checksum()` folds the real
 * algorithm's own final inversion step in directly, rather than
 * leaving it to the caller the way lwIP's own split responsibility
 * does, this chapter's own stated simplification).
 *
 * This chapter's own stated restriction, the same "known, restricted
 * schema" approach Chapters 34/40-44 used applied to a binary wire
 * format rather than a text one: IHL is always exactly 5 (a fixed
 * 20-byte header, never real IP options), and neither the real MF bit
 * nor a nonzero real fragment offset is ever accepted -- this chapter
 * builds and parses only a single, complete, unfragmented real IPv4
 * datagram, real fragmentation/reassembly being explicitly out of
 * scope. */

#define IP4_HEADER_LEN 20u
#define IP4_VERSION 4u
#define IP4_IHL_NO_OPTIONS 5u /* this chapter's own restriction: never real IP options */
#define IP4_PROTO_ICMP 1u     /* real IANA-assigned protocol number for ICMP */

/* Cited directly from lwIP's own real `IP_DF`/`IP_MF`/`IP_OFFMASK`
 * bit-mask constants over the real combined 16-bit flags+fragment-
 * offset wire field. */
#define IP4_FLAG_DF 0x4000u
#define IP4_FLAG_MF 0x2000u
#define IP4_OFFMASK 0x1fffu

typedef struct {
    uint8_t version;         /* always IP4_VERSION (4) */
    uint8_t ihl;              /* always IP4_IHL_NO_OPTIONS (5) */
    uint8_t tos;
    uint16_t total_length;
    uint16_t identification;
    uint16_t flags_offset;   /* the real combined 16-bit wire field */
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    uint8_t src_addr[4];
    uint8_t dst_addr[4];
} ip4_header_t;

/* The real RFC 1071 Internet checksum: a 16-bit one's-complement sum
 * of `len` bytes (treated as big-endian 16-bit words, one odd
 * trailing byte padded with a zero low byte), with end-around carry
 * folded back in, then inverted -- the real algorithm both IPv4's own
 * header checksum and ICMP's own checksum share (048_icmp.c reuses
 * this same function, never reimplementing it, since RFC 1071's own
 * algorithm is not IPv4-specific). */
uint16_t ip4_checksum(const uint8_t *data, uint32_t len);

/* Builds a real 20-byte IPv4 header into `out` (IP4_HEADER_LEN bytes,
 * never more -- `hdr->ihl` is ignored and always written as
 * IP4_IHL_NO_OPTIONS). Computes and fills in the real header checksum
 * itself (`hdr->checksum` is ignored on input). Returns
 * IP4_HEADER_LEN, or 0 on refusal if `out_size` is too small. */
uint32_t ip4_build_header(const ip4_header_t *hdr, uint8_t *out, uint32_t out_size);

/* Parses exactly IP4_HEADER_LEN bytes of a real IPv4 header back into
 * `*out`. Returns 1 on success, or 0 -- refusing outright -- if `len`
 * is too short, `version` is not 4, `ihl` is not IP4_IHL_NO_OPTIONS
 * (real IP options present), the real header checksum does not
 * verify, the real MF bit is set, or the real fragment offset is
 * nonzero (this chapter's own stated no-fragmentation scope). */
int ip4_parse_header(const uint8_t *buf, uint32_t len, ip4_header_t *out);

#endif
