/* See 037_arp_cache.h's own top-of-file comment for the full real
 * citation of every design decision below. */

#include <stdint.h>
#include <stddef.h>

#include "037_arp_cache.h"
#include "037_arp.h"
#include "037_pit.h"

static arp_cache_entry_t g_cache[ARP_CACHE_MAX_ENTRIES];

static int ip_equal(const uint8_t a[4], const uint8_t b[4]) {
    for (uint32_t i = 0; i < 4u; i++) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

void arp_cache_init(void) {
    for (uint32_t i = 0; i < ARP_CACHE_MAX_ENTRIES; i++) {
        g_cache[i].in_use = 0;
    }
}

void arp_cache_insert(const uint8_t ip[4], const uint8_t mac[6]) {
    uint32_t now = pit_get_ticks();

    /* Real Merge_flag logic, RFC 826's own words: "If the pair
     * <protocol type, sender protocol address> is already in my
     * translation table, update the sender hardware address field of
     * the entry ... and set Merge_flag to true." This driver only
     * ever speaks one real protocol (IPv4), so the pair collapses to
     * just the IP. */
    for (uint32_t i = 0; i < ARP_CACHE_MAX_ENTRIES; i++) {
        if (g_cache[i].in_use && ip_equal(g_cache[i].ip, ip)) {
            for (uint32_t b = 0; b < 6u; b++) {
                g_cache[i].mac[b] = mac[b];
            }
            g_cache[i].last_used_tick = now;
            return;
        }
    }

    /* Merge_flag was false: "add the triplet ... to the translation
     * table." First, a real free slot, if one exists. */
    for (uint32_t i = 0; i < ARP_CACHE_MAX_ENTRIES; i++) {
        if (!g_cache[i].in_use) {
            for (uint32_t b = 0; b < 4u; b++) {
                g_cache[i].ip[b] = ip[b];
            }
            for (uint32_t b = 0; b < 6u; b++) {
                g_cache[i].mac[b] = mac[b];
            }
            g_cache[i].last_used_tick = now;
            g_cache[i].in_use = 1;
            return;
        }
    }

    /* No real free slot: this chapter's own cited LRU eviction --
     * find the one real in-use entry with the smallest (oldest)
     * last_used_tick and overwrite it. */
    uint32_t lru_index = 0;
    uint32_t lru_tick = g_cache[0].last_used_tick;
    for (uint32_t i = 1; i < ARP_CACHE_MAX_ENTRIES; i++) {
        if (g_cache[i].last_used_tick < lru_tick) {
            lru_tick = g_cache[i].last_used_tick;
            lru_index = i;
        }
    }
    for (uint32_t b = 0; b < 4u; b++) {
        g_cache[lru_index].ip[b] = ip[b];
    }
    for (uint32_t b = 0; b < 6u; b++) {
        g_cache[lru_index].mac[b] = mac[b];
    }
    g_cache[lru_index].last_used_tick = now;
    g_cache[lru_index].in_use = 1;
}

int arp_cache_lookup(const uint8_t ip[4], uint8_t out_mac[6]) {
    uint32_t now = pit_get_ticks();

    for (uint32_t i = 0; i < ARP_CACHE_MAX_ENTRIES; i++) {
        if (!g_cache[i].in_use || !ip_equal(g_cache[i].ip, ip)) {
            continue;
        }

        /* Real expiry, this chapter's own cited RFC 1122 2.3.2.1
         * MUST: age is real elapsed PIT ticks since this entry was
         * last confirmed/used. */
        uint32_t age = now - g_cache[i].last_used_tick;
        if (age >= ARP_CACHE_ENTRY_TIMEOUT_TICKS) {
            g_cache[i].in_use = 0;
            return 0;
        }

        for (uint32_t b = 0; b < 6u; b++) {
            out_mac[b] = g_cache[i].mac[b];
        }
        g_cache[i].last_used_tick = now;
        return 1;
    }

    return 0;
}

int arp_resolve(const uint8_t *src_mac, const uint8_t src_ip[4],
                 const uint8_t target_ip[4], uint32_t max_attempts,
                 uint8_t out_mac[6], int *out_was_cache_hit) {
    if (arp_cache_lookup(target_ip, out_mac)) {
        if (out_was_cache_hit != NULL) {
            *out_was_cache_hit = 1;
        }
        return 1;
    }

    if (out_was_cache_hit != NULL) {
        *out_was_cache_hit = 0;
    }

    if (!arp_send_request(src_mac, src_ip, target_ip)) {
        return 0;
    }

    arp_packet_t reply;
    if (!arp_receive_reply(max_attempts, target_ip, &reply)) {
        return 0;
    }

    for (uint32_t b = 0; b < 6u; b++) {
        out_mac[b] = reply.sender_mac[b];
    }
    arp_cache_insert(target_ip, reply.sender_mac);
    return 1;
}
