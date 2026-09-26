/* See 033_arp.h's own top-of-file comment for the full real citation
 * of every field, value, and design decision below. */

#include <stdint.h>

#include "033_arp.h"
#include "033_rtl8139.h"

/* Real byte offsets into a real 60-byte ARP-over-Ethernet frame, all
 * cited field-for-field in 033_arp.h: a real 14-byte Ethernet header
 * (6 dst + 6 src + 2 EtherType) followed immediately by the real
 * 28-byte ARP payload RFC 826 and OSDev's own struct both describe. */
#define ETH_HDR_SIZE 14u
#define ARP_OFF_HTYPE       (ETH_HDR_SIZE + 0u)
#define ARP_OFF_PTYPE       (ETH_HDR_SIZE + 2u)
#define ARP_OFF_HLEN        (ETH_HDR_SIZE + 4u)
#define ARP_OFF_PLEN        (ETH_HDR_SIZE + 5u)
#define ARP_OFF_OPCODE      (ETH_HDR_SIZE + 6u)
#define ARP_OFF_SENDER_MAC  (ETH_HDR_SIZE + 8u)
#define ARP_OFF_SENDER_IP   (ETH_HDR_SIZE + 14u)
#define ARP_OFF_TARGET_MAC  (ETH_HDR_SIZE + 18u)
#define ARP_OFF_TARGET_IP   (ETH_HDR_SIZE + 24u)

int arp_send_request(const uint8_t *src_mac, const uint8_t src_ip[4],
                      const uint8_t target_ip[4]) {
    uint8_t frame[ARP_FRAME_SIZE];

    /* Real Ethernet header: destination broadcast (RFC 826: a request
     * "is ... broadcast to all stations on the Ethernet cable"),
     * source this machine's own real MAC, EtherType 0x0806 (cited,
     * IANA's own IEEE 802 Numbers registry). */
    for (uint32_t i = 0; i < 6u; i++) {
        frame[i] = 0xFFu;              /* destination: real broadcast */
        frame[6u + i] = src_mac[i];    /* source: this machine's own real MAC */
    }
    frame[12] = (uint8_t) (ETHERTYPE_ARP >> 8);
    frame[13] = (uint8_t) ETHERTYPE_ARP;

    /* Real ARP payload, RFC 826's own field order, each multi-byte
     * field written big-endian by hand -- this machine's own x86
     * registers are little-endian, so a native uint16_t store would
     * write the wrong real byte order onto the wire. */
    frame[ARP_OFF_HTYPE]     = (uint8_t) (ARP_HTYPE_ETHERNET >> 8);
    frame[ARP_OFF_HTYPE + 1] = (uint8_t) ARP_HTYPE_ETHERNET;
    frame[ARP_OFF_PTYPE]     = (uint8_t) (ARP_PTYPE_IPV4 >> 8);
    frame[ARP_OFF_PTYPE + 1] = (uint8_t) ARP_PTYPE_IPV4;
    frame[ARP_OFF_HLEN]      = (uint8_t) ARP_HLEN_ETHERNET;
    frame[ARP_OFF_PLEN]      = (uint8_t) ARP_PLEN_IPV4;
    frame[ARP_OFF_OPCODE]     = (uint8_t) (ARP_OP_REQUEST >> 8);
    frame[ARP_OFF_OPCODE + 1] = (uint8_t) ARP_OP_REQUEST;

    for (uint32_t i = 0; i < 6u; i++) {
        frame[ARP_OFF_SENDER_MAC + i] = src_mac[i];
        /* Real target hardware address: zeroed, cited directly --
         * OSDev: "For an ARP request most implementations zero the
         * destination MAC address." It is not known yet; that is
         * exactly what this real request is asking. */
        frame[ARP_OFF_TARGET_MAC + i] = 0x00u;
    }
    for (uint32_t i = 0; i < 4u; i++) {
        frame[ARP_OFF_SENDER_IP + i] = src_ip[i];
        frame[ARP_OFF_TARGET_IP + i] = target_ip[i];
    }

    /* Real IEEE 802.3 minimum padding (this book's own cited
     * convention since Chapter 25): the real Ethernet header plus ARP
     * payload above is only 42 bytes. */
    for (uint32_t i = ETH_HDR_SIZE + 28u; i < ARP_FRAME_SIZE; i++) {
        frame[i] = 0x00u;
    }

    /* Real fix, this chapter's own: NOT rtl8139_send() (always
     * descriptor 0). Chapter 27's own real, documented testing found
     * that retriggering the SAME descriptor a second time in a row,
     * with no other descriptor's own transmission in between, can
     * leave that second real transmission genuinely stuck (see
     * 033_rtl8139.c's own rtl8139_send() comment) -- Chapter 28 never
     * hit this, since it only ever sent one real ARP request per
     * boot, but this chapter's own new cache demo genuinely sends
     * several fresh real ARP requests across a single boot, hitting
     * exactly that real, already-documented hazard the first time
     * this driver ever called rtl8139_send() twice. Fixed by using
     * rtl8139_send_queue()'s own real round-robin descriptor
     * allocation instead (the same real fix Chapter 27 itself already
     * uses for its own repeated sends), then blocking on that exact
     * descriptor's own real completion via
     * rtl8139_wait_descriptor_sent() -- giving arp_send_request() the
     * same real single-shot, blocking contract callers already rely
     * on, just never reusing a descriptor back-to-back. */
    int desc = rtl8139_send_queue(frame, ARP_FRAME_SIZE);
    if (desc < 0) {
        return 0;
    }
    rtl8139_wait_descriptor_sent(desc);
    return 1;
}

int arp_receive_reply(uint32_t max_attempts, const uint8_t expected_sender_ip[4],
                       arp_packet_t *out_reply) {
    uint8_t rx_frame[RTL8139_MAX_FRAME];

    for (uint32_t attempt = 0; attempt < max_attempts; attempt++) {
        uint32_t rx_len = 0;
        if (!rtl8139_receive_next_packet(rx_frame, &rx_len)) {
            continue;
        }
        /* Too short to even hold a real Ethernet header plus a real
         * 28-byte ARP payload -- cannot be the reply being waited
         * for, whatever it is. */
        if (rx_len < ETH_HDR_SIZE + 28u) {
            continue;
        }

        uint16_t ethertype = (uint16_t) ((rx_frame[12] << 8) | rx_frame[13]);
        if (ethertype != ETHERTYPE_ARP) {
            continue;
        }

        uint16_t htype = (uint16_t) ((rx_frame[ARP_OFF_HTYPE] << 8) |
                                      rx_frame[ARP_OFF_HTYPE + 1]);
        uint16_t ptype = (uint16_t) ((rx_frame[ARP_OFF_PTYPE] << 8) |
                                      rx_frame[ARP_OFF_PTYPE + 1]);
        /* RFC 826's own reception algorithm opens with exactly these
         * two checks -- "Do I have the hardware type in ar$hrd? ...
         * Do I speak the protocol in ar$pro?" -- before trusting
         * anything else in the packet. This driver only ever speaks
         * one real hardware/protocol pair, so both are fixed
         * constants rather than a real lookup table. */
        if (htype != ARP_HTYPE_ETHERNET || ptype != ARP_PTYPE_IPV4 ||
            rx_frame[ARP_OFF_HLEN] != ARP_HLEN_ETHERNET ||
            rx_frame[ARP_OFF_PLEN] != ARP_PLEN_IPV4) {
            continue;
        }

        uint16_t opcode = (uint16_t) ((rx_frame[ARP_OFF_OPCODE] << 8) |
                                       rx_frame[ARP_OFF_OPCODE + 1]);
        if (opcode != ARP_OP_REPLY) {
            continue;
        }

        int sender_matches = 1;
        for (uint32_t i = 0; i < 4u; i++) {
            if (rx_frame[ARP_OFF_SENDER_IP + i] != expected_sender_ip[i]) {
                sender_matches = 0;
                break;
            }
        }
        if (!sender_matches) {
            continue;
        }

        out_reply->htype = htype;
        out_reply->ptype = ptype;
        out_reply->hlen = rx_frame[ARP_OFF_HLEN];
        out_reply->plen = rx_frame[ARP_OFF_PLEN];
        out_reply->opcode = opcode;
        for (uint32_t i = 0; i < 6u; i++) {
            out_reply->sender_mac[i] = rx_frame[ARP_OFF_SENDER_MAC + i];
            out_reply->target_mac[i] = rx_frame[ARP_OFF_TARGET_MAC + i];
        }
        for (uint32_t i = 0; i < 4u; i++) {
            out_reply->sender_ip[i] = rx_frame[ARP_OFF_SENDER_IP + i];
            out_reply->target_ip[i] = rx_frame[ARP_OFF_TARGET_IP + i];
        }
        return 1;
    }

    return 0;
}
