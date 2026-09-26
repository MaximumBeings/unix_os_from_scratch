/* See 040_pinpad.h's own top-of-file comment on what this boundary
 * does and does not enforce. */

#include "040_pinpad.h"
#include "040_pinblock.h"

int pinpad_capture(const uint8_t *pin, uint32_t pin_len, const uint8_t *pan, uint32_t pan_len,
                    pinpad_result_t *out) {
    uint8_t local_pin[PINBLOCK_MAX_PIN_LEN];
    uint32_t copy_len = (pin_len < PINBLOCK_MAX_PIN_LEN) ? pin_len : PINBLOCK_MAX_PIN_LEN;
    for (uint32_t i = 0; i < copy_len; i++) {
        local_pin[i] = pin[i];
    }

    int ok = pinblock_build_format0(local_pin, pin_len, pan, pan_len, out->pin_block);

    /* The PIN never crosses back out of this function -- every local
     * trace of it is overwritten here, whether or not the block build
     * succeeded. */
    for (uint32_t i = 0; i < PINBLOCK_MAX_PIN_LEN; i++) {
        local_pin[i] = 0;
    }
    return ok;
}
