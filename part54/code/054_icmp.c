/* See 054_icmp.h's own top-of-file comment for the citation of every
 * real field name, value, and the real RFC 1071 checksum algorithm
 * (reused from 054_ip.c, never reimplemented) used here. */

#include "054_icmp.h"
#include "054_ip.h"
#include "054_rtl8139.h"

uint32_t icmp_build_echo(const icmp_echo_t *echo, uint8_t *out, uint32_t out_size) {
    if (echo->payload_len > ICMP_ECHO_PAYLOAD_MAX) {
        return 0;
    }
    uint32_t total = ICMP_ECHO_HEADER_LEN + echo->payload_len;
    if (out_size < total) {
        return 0;
    }
    out[0] = echo->type;
    out[1] = echo->code;
    out[2] = 0; /* checksum placeholder, filled in below */
    out[3] = 0;
    out[4] = (uint8_t)(echo->identifier >> 8);
    out[5] = (uint8_t) echo->identifier;
    out[6] = (uint8_t)(echo->sequence >> 8);
    out[7] = (uint8_t) echo->sequence;
    for (uint32_t i = 0; i < echo->payload_len; i++) {
        out[ICMP_ECHO_HEADER_LEN + i] = echo->payload[i];
    }
    uint16_t checksum = ip4_checksum(out, total);
    out[2] = (uint8_t)(checksum >> 8);
    out[3] = (uint8_t) checksum;
    return total;
}

int icmp_parse_echo(const uint8_t *buf, uint32_t len, icmp_echo_t *out) {
    if (len < ICMP_ECHO_HEADER_LEN) {
        return 0;
    }
    uint32_t payload_len = len - ICMP_ECHO_HEADER_LEN;
    if (payload_len > ICMP_ECHO_PAYLOAD_MAX) {
        return 0;
    }
    uint8_t copy[ICMP_ECHO_HEADER_LEN + ICMP_ECHO_PAYLOAD_MAX];
    for (uint32_t i = 0; i < len; i++) {
        copy[i] = buf[i];
    }
    uint16_t received_checksum = (uint16_t)(((uint16_t) buf[2] << 8) | (uint16_t) buf[3]);
    copy[2] = 0;
    copy[3] = 0;
    uint16_t computed_checksum = ip4_checksum(copy, len);
    if (received_checksum != computed_checksum) {
        return 0;
    }
    out->type = buf[0];
    out->code = buf[1];
    out->checksum = received_checksum;
    out->identifier = (uint16_t)(((uint16_t) buf[4] << 8) | (uint16_t) buf[5]);
    out->sequence = (uint16_t)(((uint16_t) buf[6] << 8) | (uint16_t) buf[7]);
    out->payload_len = payload_len;
    for (uint32_t i = 0; i < payload_len; i++) {
        out->payload[i] = buf[ICMP_ECHO_HEADER_LEN + i];
    }
    return 1;
}

#define ICMP_FRAME_MAX (14u + IP4_HEADER_LEN + ICMP_ECHO_HEADER_LEN + ICMP_ECHO_PAYLOAD_MAX)

static uint8_t g_icmp_tx[ICMP_FRAME_MAX];
static uint8_t g_icmp_rx[RTL8139_MAX_FRAME];

int icmp_send_echo_request(const uint8_t *src_mac, const uint8_t src_ip[4],
                            const uint8_t dst_mac[6], const uint8_t dst_ip[4],
                            uint16_t identifier, uint16_t sequence, const uint8_t *payload,
                            uint32_t payload_len) {
    if (payload_len > ICMP_ECHO_PAYLOAD_MAX) {
        return 0;
    }
    icmp_echo_t echo;
    echo.type = ICMP_TYPE_ECHO_REQUEST;
    echo.code = 0;
    echo.identifier = identifier;
    echo.sequence = sequence;
    echo.payload_len = payload_len;
    for (uint32_t i = 0; i < payload_len; i++) {
        echo.payload[i] = payload[i];
    }

    uint8_t icmp_buf[ICMP_ECHO_HEADER_LEN + ICMP_ECHO_PAYLOAD_MAX];
    uint32_t icmp_len = icmp_build_echo(&echo, icmp_buf, sizeof(icmp_buf));
    if (icmp_len == 0) {
        return 0;
    }

    ip4_header_t ip_hdr;
    ip_hdr.tos = 0;
    ip_hdr.total_length = (uint16_t)(IP4_HEADER_LEN + icmp_len);
    ip_hdr.identification = identifier;
    ip_hdr.flags_offset = IP4_FLAG_DF;
    ip_hdr.ttl = 64; /* this chapter's own chosen value -- RFC 791 states no
                       * single mandatory default */
    ip_hdr.protocol = IP4_PROTO_ICMP;
    for (int i = 0; i < 4; i++) {
        ip_hdr.src_addr[i] = src_ip[i];
        ip_hdr.dst_addr[i] = dst_ip[i];
    }

    for (int i = 0; i < 6; i++) {
        g_icmp_tx[i] = dst_mac[i];
        g_icmp_tx[6 + i] = src_mac[i];
    }
    g_icmp_tx[12] = (uint8_t)(ETHERTYPE_IPV4 >> 8);
    g_icmp_tx[13] = (uint8_t) ETHERTYPE_IPV4;
    uint32_t ip_written = ip4_build_header(&ip_hdr, &g_icmp_tx[14], sizeof(g_icmp_tx) - 14u);
    if (ip_written != IP4_HEADER_LEN) {
        return 0;
    }
    for (uint32_t i = 0; i < icmp_len; i++) {
        g_icmp_tx[14u + IP4_HEADER_LEN + i] = icmp_buf[i];
    }
    uint32_t frame_len = 14u + IP4_HEADER_LEN + icmp_len;
    /* Real IEEE 802.3 minimum frame size, cited since Chapter 25 --
     * see 054_kmain.c's own DEMO_FRAME_SIZE comment. */
    uint32_t padded_len = frame_len < 60u ? 60u : frame_len;
    for (uint32_t i = frame_len; i < padded_len; i++) {
        g_icmp_tx[i] = 0;
    }

    int desc = rtl8139_send_queue(g_icmp_tx, padded_len);
    if (desc < 0) {
        return 0;
    }
    rtl8139_wait_descriptor_sent(desc);
    return 1;
}

int icmp_receive_echo_reply(uint32_t max_attempts, const uint8_t expected_src_ip[4],
                             uint16_t expected_identifier, uint16_t expected_sequence,
                             icmp_echo_t *out_reply) {
    for (uint32_t attempt = 0; attempt < max_attempts; attempt++) {
        uint32_t rx_len = 0;
        if (!rtl8139_receive_next_packet(g_icmp_rx, &rx_len)) {
            continue;
        }
        if (rx_len < 14u + IP4_HEADER_LEN) {
            continue;
        }
        if (g_icmp_rx[12] != (uint8_t)(ETHERTYPE_IPV4 >> 8) ||
            g_icmp_rx[13] != (uint8_t) ETHERTYPE_IPV4) {
            continue;
        }
        ip4_header_t ip_hdr;
        if (!ip4_parse_header(&g_icmp_rx[14], rx_len - 14u, &ip_hdr)) {
            continue;
        }
        if (ip_hdr.protocol != IP4_PROTO_ICMP) {
            continue;
        }
        int src_ok = 1;
        for (int i = 0; i < 4; i++) {
            if (ip_hdr.src_addr[i] != expected_src_ip[i]) {
                src_ok = 0;
            }
        }
        if (!src_ok) {
            continue;
        }
        uint32_t icmp_offset = 14u + IP4_HEADER_LEN;
        if (ip_hdr.total_length < IP4_HEADER_LEN ||
            icmp_offset + (ip_hdr.total_length - IP4_HEADER_LEN) > rx_len) {
            continue;
        }
        uint32_t icmp_len = ip_hdr.total_length - IP4_HEADER_LEN;
        icmp_echo_t echo;
        if (!icmp_parse_echo(&g_icmp_rx[icmp_offset], icmp_len, &echo)) {
            continue;
        }
        if (echo.type != ICMP_TYPE_ECHO_REPLY || echo.identifier != expected_identifier ||
            echo.sequence != expected_sequence) {
            continue;
        }
        *out_reply = echo;
        return 1;
    }
    return 0;
}
