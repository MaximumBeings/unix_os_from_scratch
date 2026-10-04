/* See 053_ip.h's own top-of-file comment for the citation of every
 * real field name, bit layout, and the real RFC 1071 checksum
 * algorithm used here. */

#include "053_ip.h"

uint16_t ip4_checksum(const uint8_t *data, uint32_t len) {
    uint32_t acc = 0;
    uint32_t i = 0;
    while (i + 1u < len) {
        uint16_t word = (uint16_t)(((uint16_t) data[i] << 8) | (uint16_t) data[i + 1u]);
        acc += word;
        i += 2u;
    }
    if (i < len) {
        acc += (uint32_t)((uint16_t) data[i] << 8);
    }
    acc = (acc >> 16) + (acc & 0xFFFFu);
    if ((acc & 0xFFFF0000u) != 0u) {
        acc = (acc >> 16) + (acc & 0xFFFFu);
    }
    return (uint16_t) ~((uint16_t) acc);
}

uint32_t ip4_build_header(const ip4_header_t *hdr, uint8_t *out, uint32_t out_size) {
    if (out_size < IP4_HEADER_LEN) {
        return 0;
    }
    out[0] = (uint8_t)((IP4_VERSION << 4) | IP4_IHL_NO_OPTIONS);
    out[1] = hdr->tos;
    out[2] = (uint8_t)(hdr->total_length >> 8);
    out[3] = (uint8_t) hdr->total_length;
    out[4] = (uint8_t)(hdr->identification >> 8);
    out[5] = (uint8_t) hdr->identification;
    out[6] = (uint8_t)(hdr->flags_offset >> 8);
    out[7] = (uint8_t) hdr->flags_offset;
    out[8] = hdr->ttl;
    out[9] = hdr->protocol;
    out[10] = 0; /* checksum placeholder, filled in below */
    out[11] = 0;
    for (int i = 0; i < 4; i++) {
        out[12 + i] = hdr->src_addr[i];
        out[16 + i] = hdr->dst_addr[i];
    }
    uint16_t checksum = ip4_checksum(out, IP4_HEADER_LEN);
    out[10] = (uint8_t)(checksum >> 8);
    out[11] = (uint8_t) checksum;
    return IP4_HEADER_LEN;
}

int ip4_parse_header(const uint8_t *buf, uint32_t len, ip4_header_t *out) {
    if (len < IP4_HEADER_LEN) {
        return 0;
    }
    uint8_t version = (uint8_t)(buf[0] >> 4);
    uint8_t ihl = (uint8_t)(buf[0] & 0x0Fu);
    if (version != IP4_VERSION || ihl != IP4_IHL_NO_OPTIONS) {
        return 0;
    }
    uint8_t header_copy[IP4_HEADER_LEN];
    for (uint32_t i = 0; i < IP4_HEADER_LEN; i++) {
        header_copy[i] = buf[i];
    }
    header_copy[10] = 0;
    header_copy[11] = 0;
    uint16_t received_checksum = (uint16_t)(((uint16_t) buf[10] << 8) | (uint16_t) buf[11]);
    uint16_t computed_checksum = ip4_checksum(header_copy, IP4_HEADER_LEN);
    if (received_checksum != computed_checksum) {
        return 0;
    }
    uint16_t flags_offset = (uint16_t)(((uint16_t) buf[6] << 8) | (uint16_t) buf[7]);
    if ((flags_offset & IP4_FLAG_MF) != 0u || (flags_offset & IP4_OFFMASK) != 0u) {
        return 0;
    }
    out->version = version;
    out->ihl = ihl;
    out->tos = buf[1];
    out->total_length = (uint16_t)(((uint16_t) buf[2] << 8) | (uint16_t) buf[3]);
    out->identification = (uint16_t)(((uint16_t) buf[4] << 8) | (uint16_t) buf[5]);
    out->flags_offset = flags_offset;
    out->ttl = buf[8];
    out->protocol = buf[9];
    out->checksum = received_checksum;
    for (int i = 0; i < 4; i++) {
        out->src_addr[i] = buf[12 + i];
        out->dst_addr[i] = buf[16 + i];
    }
    return 1;
}
