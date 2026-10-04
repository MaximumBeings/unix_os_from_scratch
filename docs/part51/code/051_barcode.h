#ifndef UNIX_OS_051_BARCODE_H
#define UNIX_OS_051_BARCODE_H

#include <stdint.h>
#include "051_gs1.h"

/* This chapter's own rotating anti-fraud barcode -- unlike 051_gs1.h's
 * own real GS1 GDTI citation, this file's own CONSTRUCTION is
 * explicitly invented, stated plainly, the same honesty note
 * 039_emv.h gave its own substitute EMV cryptogram. What is real is
 * the PROPERTY it reproduces: Ticketmaster's own real "SafeTix"
 * technology is publicly described (through search results, since
 * conduition.io's own real reverse-engineering write-up and
 * ticketmaster.com itself are both blocked by this sandbox's egress
 * policy) as a barcode that "automatically refreshes every 15
 * seconds," specifically to defeat screenshot/duplicate fraud -- the
 * real screenshot becomes worthless once the next real rotation
 * happens, because a venue scanner checks not just the barcode's own
 * data but a real timestamp and a real cryptographic proof that the
 * code was generated recently.
 *
 * SafeTix's own real rotation algorithm is proprietary (and, per a
 * real, recent lawsuit found through this same search, contested as a
 * patented trade secret) -- this book has no access to it and does not
 * claim to reproduce it. `barcode_rotating_code()` below is this
 * book's own substitute, built entirely from primitives this kernel
 * already has: a real HMAC-SHA256 (051_hmac.h, since Chapter 30) over
 * the real GS1 GDTI (051_gs1.h) plus a real 15-second epoch counter,
 * truncated to a short hex code. It occupies the same real place in
 * the same real anti-fraud flow -- a code a venue scanner can verify
 * was generated within the last real epoch or two, keyed by a secret
 * only the ticketing system and its own scanners hold -- without
 * claiming to be SafeTix's own real, undisclosed construction. */

#define BARCODE_EPOCH_SECONDS 15u /* real, cited SafeTix refresh interval */
#define BARCODE_CODE_LEN 8u /* this book's own invented truncation width,
                             * ASCII hex characters (4 real HMAC bytes) */

/* Computes this chapter's own rotating code for `ticket` at real epoch
 * number `epoch` (real wall-clock seconds / BARCODE_EPOCH_SECONDS),
 * keyed by `key`/`key_len`, into `out_code` (BARCODE_CODE_LEN ASCII
 * hex characters, not NUL-terminated). */
void barcode_rotating_code(const gs1_ticket_id_t *ticket, uint32_t epoch, const uint8_t *key,
                            uint32_t key_len, uint8_t out_code[BARCODE_CODE_LEN]);

/* Verifies `received_code` against the expected code for `ticket` at
 * any epoch from `current_epoch - tolerance_epochs` through
 * `current_epoch` inclusive (real allowance for a real scanner's own
 * clock/network skew, this book's own stated choice of how much). *
 * Returns 1 if `received_code` matches at any epoch in that window, 0
 * otherwise -- including a screenshot of an OLDER code once it falls
 * outside the tolerance window, the real property this whole
 * mechanism exists to enforce. */
int barcode_verify(const uint8_t received_code[BARCODE_CODE_LEN], const gs1_ticket_id_t *ticket,
                    uint32_t current_epoch, const uint8_t *key, uint32_t key_len,
                    uint32_t tolerance_epochs);

#endif
