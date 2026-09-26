#ifndef UNIX_OS_041_PINPAD_H
#define UNIX_OS_041_PINPAD_H

#include <stdint.h>

/* This chapter's own PIN-pad isolation boundary -- not a real cited
 * external format (there is no standard this book could cite for "how
 * a PIN pad talks to a payment application"; every real vendor's own
 * internal protocol here is proprietary). What is real, and what this
 * file's own API is designed to enforce, is the real security
 * PRINCIPLE the confirmed scope for this chapter names explicitly: a
 * real PCI PTS-certified PIN pad's own secure processor never lets a
 * cleartext PIN leave its own boundary -- only an already-encrypted
 * PIN block crosses into the rest of a real POS terminal's own
 * software.
 *
 * This kernel has no separate secure element, no second processor, and
 * no hardware boundary to enforce that with -- stated plainly, the
 * same honesty note 038_atm.h/.c gave for its own invented account
 * store. What this file DOES enforce, at the API level: no function
 * anywhere in this codebase accepts or returns a raw PIN once
 * pinpad_capture() has run. The PIN digits live only in a local stack
 * buffer inside pinpad_capture() itself, explicitly zeroed before it
 * returns -- the only thing that ever crosses back out is the finished
 * ISO 9564-1 Format 0 PIN block (041_pinblock.h), the same real block
 * this book has built since Chapter 38. A real hardware boundary is
 * enforced by physical tamper-resistance and a separate secure
 * processor; this one is enforced by never having a code path that
 * could return the raw PIN at all. */

typedef struct {
    uint8_t pin_block[8];
} pinpad_result_t;

/* Builds the real Format 0 PIN block from `pin`/`pin_len` and
 * `pan`/`pan_len` (041_pinblock.h's own pinblock_build_format0()),
 * zeroing every local copy of `pin` before returning regardless of
 * success or failure. Returns 1 on success, or 0 if
 * pinblock_build_format0() itself refuses (a PIN/PAN length outside
 * its own stated bounds). `out->pin_block` is left unmodified on
 * failure. */
int pinpad_capture(const uint8_t *pin, uint32_t pin_len, const uint8_t *pan, uint32_t pan_len,
                    pinpad_result_t *out);

#endif
