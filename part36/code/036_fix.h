#ifndef UNIX_OS_036_FIX_H
#define UNIX_OS_036_FIX_H

#include <stdint.h>

/* A real FIX (Financial Information eXchange) protocol message
 * encoder/decoder -- the real, industry-standard tag=value protocol
 * electronic trading systems actually use to send orders and receive
 * fills. FIX's own official site, fixtrading.org, is blocked by this
 * sandbox's network egress policy (the same honesty note Chapters 33
 * and 34 made about their own blocked standards sites), but a real,
 * widely-used open-source FIX engine's own data dictionary was reached
 * and read in full instead: a clone of github.com/quickfix/quickfix,
 * `spec/FIX44.xml` -- the real FIX 4.4 protocol dictionary that
 * project's own engine is generated from, not a summary of it. Every
 * tag number, message field list, and enumerated value below was read
 * directly out of that real file, the same discipline Chapter 34 used
 * for its own ACORD sample and Chapter 32 used for NACHA.
 *
 * Real tag numbers this chapter uses, cited directly from FIX44.xml's
 * own <field number='N' name='...'> declarations: BeginString (8),
 * BodyLength (9), MsgType (35), SenderCompID (49), TargetCompID (56),
 * MsgSeqNum (34), SendingTime (52), CheckSum (10) -- the real FIX
 * "standard header"/"standard trailer" fields every message carries;
 * ClOrdID (11), Account (1), HandlInst (21), Symbol (55), Side (54),
 * TransactTime (60), OrdType (40), OrderQty (38), TimeInForce (59) --
 * the real NewOrderSingle (MsgType 'D') fields this chapter's own
 * request uses; and OrderID (37), ExecID (17), ExecType (150),
 * OrdStatus (39), LeavesQty (151), CumQty (14), AvgPx (6), LastQty
 * (32), LastPx (31) -- the real ExecutionReport (MsgType '8') fields
 * this chapter's own response uses. Real enumerated values, likewise
 * cited from FIX44.xml's own <value enum='N' description='...'>
 * children of each field: Side '1' (BUY); OrdType '1' (MARKET);
 * TimeInForce '0' (DAY); HandlInst '1' (AUTOMATED_EXECUTION_NO_
 * INTERVENTION); ExecType 'F' (TRADE); OrdStatus '2' (FILLED).
 *
 * Real FIX framing, cited from the same file's own header-field
 * ordering and from general knowledge of the protocol's own wire
 * format (BodyLength/CheckSum computation is a mechanical consequence
 * of the field layout, not something FIX44.xml states as prose): tag
 * 9 (BodyLength) holds the exact byte count from the first byte after
 * its own trailing SOH delimiter through the last byte of the field
 * immediately before tag 10 (CheckSum); tag 10 holds the sum of every
 * byte from the start of the message (tag 8's own '8') through the SOH
 * immediately before tag 10, taken modulo 256, printed as exactly 3
 * zero-padded decimal digits. `fix_build_message()`/`fix_parse_message()`
 * compute and verify both fields for real, never trusting a caller (or
 * a received message) to have gotten them right.
 *
 * Real FIX delimits every field with an ASCII SOH byte (0x01), not a
 * printable character -- and this chapter's own encoder/decoder use
 * real SOH bytes on the wire, unchanged. Where this chapter's own
 * kernel demo PRINTS a FIX message to the serial log for a human to
 * read, it substitutes the printable pipe character '|' for SOH, a
 * common, real convention FIX documentation and tools themselves use
 * for human-readable display (SOH does not render visibly in a
 * terminal) -- stated here as a display-only substitution, never a
 * wire-format change: `fix_build_message()` itself always emits real
 * SOH bytes, and `fix_parse_message()` only ever accepts them.
 *
 * This chapter's own invention, stated plainly: every fictional
 * fund/ticker symbol, its fictional NAV (net asset value per share),
 * every SenderCompID/TargetCompID/ClOrdID/OrderID/ExecID value, and
 * the whole round-up/risk-questionnaire/allocation scenario -- see
 * 036_investing.h. None of that is part of the real FIX protocol
 * itself; FIX only specifies the message SHAPE this chapter's own
 * scenario is carried inside. */

#define FIX_MAX_FIELDS 32u
#define FIX_MAX_TAG_VALUE_LEN 32u
#define FIX_MAX_MESSAGE_LEN 512u

typedef struct {
    uint32_t tag;
    char value[FIX_MAX_TAG_VALUE_LEN];
} fix_field_t;

typedef struct {
    char msg_type[2]; /* one ASCII character + NUL, e.g. "D" or "8" */
    fix_field_t fields[FIX_MAX_FIELDS]; /* every field EXCEPT 8/9/35/10,
                                          * which fix_build_message()/
                                          * fix_parse_message() handle
                                          * themselves */
    uint32_t field_count;
} fix_message_t;

void fix_set_field(fix_message_t *msg, uint32_t tag, const char *value);
/* Returns 0 (not found) or 1, writing the field's own value into
 * `out` (FIX_MAX_TAG_VALUE_LEN bytes) on success. */
int fix_get_field(const fix_message_t *msg, uint32_t tag, char *out);

/* Builds a real FIX 4.4 message: "8=FIX.4.4|9=<len>|35=<type>|" then
 * every field in `msg->fields`, in the order they were set, then
 * "10=<checksum>|" -- with real SOH (0x01) delimiters, computing a
 * real BodyLength and CheckSum. `sender_comp_id`/`target_comp_id` are
 * this chapter's own fictional identifiers (real tags 49/56).
 * Returns the encoded length, or 0 if `out_size` is too small or
 * `msg->msg_type` is not exactly one character. */
uint32_t fix_build_message(const fix_message_t *msg, const char *sender_comp_id,
                          const char *target_comp_id, uint32_t seq_num,
                          uint8_t *out, uint32_t out_size);

/* Parses exactly `len` bytes. Returns 1 on success, or 0 -- refusing
 * outright -- if the message does not begin with "8=FIX.4.4", if
 * BodyLength or CheckSum do not match what this decoder itself
 * recomputes, if any field is malformed (no '=', or a value too long
 * for FIX_MAX_TAG_VALUE_LEN), or if there are more than
 * FIX_MAX_FIELDS non-header/trailer fields. */
int fix_parse_message(const uint8_t *buf, uint32_t len, fix_message_t *out_msg);

#endif
