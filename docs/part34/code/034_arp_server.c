/* See 034_arp_server.h's own top-of-file comment for the full real
 * citation of the RFC 826 branch implemented here. */

#include <stdint.h>

#include "034_arp_server.h"
#include "034_arp.h"
#include "034_rtl8139.h"

/* Same real byte offsets 034_arp.c's own top-of-file already
 * establishes for a real 60-byte ARP-over-Ethernet frame -- repeated
 * here (not shared via a header) since neither file currently exposes
 * them outside its own translation unit, and this chapter does not
 * change that. */
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

int arp_server_handle_frame(const uint8_t *rx_frame, uint32_t rx_len,
                             const uint8_t *my_mac, const uint8_t my_ip[4]) {
    /* Too short to even hold a real Ethernet header plus a real
     * 28-byte ARP payload -- cannot be a real request this kernel
     * could answer. */
    if (rx_len < ETH_HDR_SIZE + 28u) {
        return 0;
    }

    uint16_t ethertype = (uint16_t) ((rx_frame[12] << 8) | rx_frame[13]);
    if (ethertype != ETHERTYPE_ARP) {
        return 0;
    }

    /* RFC 826's own opening two checks -- "Do I have the hardware
     * type in ar$hrd? ... Do I speak the protocol in ar$pro?" --
     * before trusting anything else in the packet. This kernel only
     * ever speaks one real hardware/protocol pair. */
    uint16_t htype = (uint16_t) ((rx_frame[ARP_OFF_HTYPE] << 8) |
                                  rx_frame[ARP_OFF_HTYPE + 1]);
    uint16_t ptype = (uint16_t) ((rx_frame[ARP_OFF_PTYPE] << 8) |
                                  rx_frame[ARP_OFF_PTYPE + 1]);
    if (htype != ARP_HTYPE_ETHERNET || ptype != ARP_PTYPE_IPV4 ||
        rx_frame[ARP_OFF_HLEN] != ARP_HLEN_ETHERNET ||
        rx_frame[ARP_OFF_PLEN] != ARP_PLEN_IPV4) {
        return 0;
    }

    /* "?Is the opcode ares_op$REQUEST?" -- this chapter only ever
     * answers real requests, never reacts to a reply or any other
     * real opcode landing in this kernel's own receive ring. */
    uint16_t opcode = (uint16_t) ((rx_frame[ARP_OFF_OPCODE] << 8) |
                                   rx_frame[ARP_OFF_OPCODE + 1]);
    if (opcode != ARP_OP_REQUEST) {
        return 0;
    }

    /* "?Am I the target protocol address?" -- this kernel's own real
     * refusal boundary: a request for any OTHER real IP is not this
     * kernel's to answer, and gets no reply at all, not a best-effort
     * or wrong one. */
    for (uint32_t i = 0; i < 4u; i++) {
        if (rx_frame[ARP_OFF_TARGET_IP + i] != my_ip[i]) {
            return 0;
        }
    }

    /* Yes on both counts: build the real reply. RFC 826's own words --
     * "Swap hardware and protocol fields, putting the local hardware
     * and protocol addresses in the sender fields" -- the OLD sender
     * (the real requester) becomes the NEW reply's own target; this
     * kernel's own real MAC/IP become the NEW reply's own sender. */
    uint8_t requester_mac[6];
    uint8_t requester_ip[4];
    for (uint32_t i = 0; i < 6u; i++) {
        requester_mac[i] = rx_frame[ARP_OFF_SENDER_MAC + i];
    }
    for (uint32_t i = 0; i < 4u; i++) {
        requester_ip[i] = rx_frame[ARP_OFF_SENDER_IP + i];
    }

    uint8_t reply[ARP_FRAME_SIZE];

    /* Real Ethernet header: "Send the packet to the (new) target
     * hardware address" -- the real requester's own MAC, never
     * broadcast (unlike a request, a reply is unicast straight back
     * to whoever asked). Source: this kernel's own real MAC. */
    for (uint32_t i = 0; i < 6u; i++) {
        reply[i] = requester_mac[i];
        reply[6u + i] = my_mac[i];
    }
    reply[12] = (uint8_t) (ETHERTYPE_ARP >> 8);
    reply[13] = (uint8_t) ETHERTYPE_ARP;

    reply[ARP_OFF_HTYPE]     = (uint8_t) (ARP_HTYPE_ETHERNET >> 8);
    reply[ARP_OFF_HTYPE + 1] = (uint8_t) ARP_HTYPE_ETHERNET;
    reply[ARP_OFF_PTYPE]     = (uint8_t) (ARP_PTYPE_IPV4 >> 8);
    reply[ARP_OFF_PTYPE + 1] = (uint8_t) ARP_PTYPE_IPV4;
    reply[ARP_OFF_HLEN]      = (uint8_t) ARP_HLEN_ETHERNET;
    reply[ARP_OFF_PLEN]      = (uint8_t) ARP_PLEN_IPV4;
    /* "Set the ar$op field to ares_op$REPLY." */
    reply[ARP_OFF_OPCODE]     = (uint8_t) (ARP_OP_REPLY >> 8);
    reply[ARP_OFF_OPCODE + 1] = (uint8_t) ARP_OP_REPLY;

    for (uint32_t i = 0; i < 6u; i++) {
        reply[ARP_OFF_SENDER_MAC + i] = my_mac[i];        /* new sender: this kernel */
        reply[ARP_OFF_TARGET_MAC + i] = requester_mac[i]; /* new target: the real requester */
    }
    for (uint32_t i = 0; i < 4u; i++) {
        reply[ARP_OFF_SENDER_IP + i] = my_ip[i];
        reply[ARP_OFF_TARGET_IP + i] = requester_ip[i];
    }

    /* Real IEEE 802.3 minimum padding, same real convention every
     * frame this book builds has used since Chapter 25. */
    for (uint32_t i = ETH_HDR_SIZE + 28u; i < ARP_FRAME_SIZE; i++) {
        reply[i] = 0x00u;
    }

    /* Same real fix 034_arp.c's own arp_send_request() already
     * established: rtl8139_send_queue()'s own round-robin descriptor
     * allocation, never the single-descriptor rtl8139_send(), to
     * avoid Chapter 27's own already-documented same-descriptor-twice
     * hang. */
    int desc = rtl8139_send_queue(reply, ARP_FRAME_SIZE);
    if (desc < 0) {
        return 0;
    }
    rtl8139_wait_descriptor_sent(desc);
    return 1;
}
