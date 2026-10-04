/* See 055_barcode.h's own top-of-file comment on what this rotating
 * code is (and is not) a citation of. */

#include "055_barcode.h"
#include "055_hmac.h"

static const char g_hex[] = "0123456789ABCDEF";

void barcode_rotating_code(const gs1_ticket_id_t *ticket, uint32_t epoch, const uint8_t *key,
                            uint32_t key_len, uint8_t out_code[BARCODE_CODE_LEN]) {
    uint8_t msg[GS1_GDTI_LEN + GS1_SERIAL_MAX + 4u];
    uint32_t pos = 0;
    for (uint32_t i = 0; i < GS1_GDTI_LEN; i++) {
        msg[pos++] = ticket->gdti[i];
    }
    for (uint32_t i = 0; i < ticket->serial_len; i++) {
        msg[pos++] = ticket->serial[i];
    }
    msg[pos++] = (uint8_t)(epoch >> 24);
    msg[pos++] = (uint8_t)(epoch >> 16);
    msg[pos++] = (uint8_t)(epoch >> 8);
    msg[pos++] = (uint8_t) epoch;

    uint8_t tag[HMAC_SHA256_TAG_SIZE];
    hmac_sha256(key, key_len, msg, pos, tag);
    for (uint32_t i = 0; i < BARCODE_CODE_LEN / 2u; i++) {
        out_code[2u * i] = (uint8_t) g_hex[(tag[i] >> 4) & 0xFu];
        out_code[2u * i + 1u] = (uint8_t) g_hex[tag[i] & 0xFu];
    }
}

static int bytes_eq(const uint8_t *a, const uint8_t *b, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

int barcode_verify(const uint8_t received_code[BARCODE_CODE_LEN], const gs1_ticket_id_t *ticket,
                    uint32_t current_epoch, const uint8_t *key, uint32_t key_len,
                    uint32_t tolerance_epochs) {
    for (uint32_t back = 0; back <= tolerance_epochs; back++) {
        if (back > current_epoch) {
            break; /* epochs are unsigned; refuse to wrap below zero */
        }
        uint8_t expected[BARCODE_CODE_LEN];
        barcode_rotating_code(ticket, current_epoch - back, key, key_len, expected);
        if (bytes_eq(received_code, expected, BARCODE_CODE_LEN)) {
            return 1;
        }
    }
    return 0;
}
