/* See 041_pinblock.h's own top-of-file comment for the citation and the
 * independent cross-check of every nibble laid out here. */

#include "041_pinblock.h"

static int is_digit(uint8_t c) {
    return c >= (uint8_t)'0' && c <= (uint8_t)'9';
}

static uint8_t digit_value(uint8_t c) {
    return (uint8_t)(c - (uint8_t)'0');
}

int pinblock_build_format0(const uint8_t *pin, uint32_t pin_len, const uint8_t *pan, uint32_t pan_len,
                            uint8_t out8[8]) {
    if (pin_len < PINBLOCK_MIN_PIN_LEN || pin_len > PINBLOCK_MAX_PIN_LEN) {
        return 0;
    }
    for (uint32_t i = 0; i < pin_len; i++) {
        if (!is_digit(pin[i])) {
            return 0;
        }
    }
    if (pan_len < PINBLOCK_PAN_DIGITS + 1u) {
        return 0; /* not enough digits for 12 plus its own check digit */
    }

    /* The PIN field's own 16 nibbles: control(0), length, PIN digits,
     * fill with 0xF. */
    uint8_t pin_nibbles[16];
    pin_nibbles[0] = 0x0u;
    pin_nibbles[1] = (uint8_t)pin_len;
    for (uint32_t i = 0; i < pin_len; i++) {
        pin_nibbles[2u + i] = digit_value(pin[i]);
    }
    for (uint32_t i = 2u + pin_len; i < 16u; i++) {
        pin_nibbles[i] = 0xFu;
    }

    /* The PAN field's own 16 nibbles: 4 zero nibbles, then the
     * rightmost 12 digits of the PAN, excluding its own check digit. */
    uint32_t pan_start = pan_len - (PINBLOCK_PAN_DIGITS + 1u);
    uint8_t pan_nibbles[16];
    for (uint32_t i = 0; i < 4u; i++) {
        pan_nibbles[i] = 0x0u;
    }
    for (uint32_t i = 0; i < PINBLOCK_PAN_DIGITS; i++) {
        uint8_t c = pan[pan_start + i];
        if (!is_digit(c)) {
            return 0;
        }
        pan_nibbles[4u + i] = digit_value(c);
    }

    for (uint32_t i = 0; i < 8u; i++) {
        uint8_t hi = (uint8_t)(pin_nibbles[2u * i] ^ pan_nibbles[2u * i]);
        uint8_t lo = (uint8_t)(pin_nibbles[2u * i + 1u] ^ pan_nibbles[2u * i + 1u]);
        out8[i] = (uint8_t)((hi << 4) | lo);
    }
    return 1;
}

int pinblock_verify_format0(const uint8_t block[8], const uint8_t *pin, uint32_t pin_len, const uint8_t *pan,
                             uint32_t pan_len) {
    uint8_t expected[8];
    if (!pinblock_build_format0(pin, pin_len, pan, pan_len, expected)) {
        return 0;
    }
    for (uint32_t i = 0; i < 8u; i++) {
        if (block[i] != expected[i]) {
            return 0;
        }
    }
    return 1;
}
