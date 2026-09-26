#ifndef UNIX_OS_036_OFX_H
#define UNIX_OS_036_OFX_H

#include <stdint.h>

/* A real OFX (Open Financial Exchange) bank-statement-download message
 * encoder/decoder -- the real, publicly used format banks actually
 * hand to personal-finance apps to let them download transaction
 * history, exactly this chapter's own scenario. OFX's own official
 * site, ofx.net, is blocked by this sandbox's network egress policy --
 * the same honesty note Chapters 33-35 made about their own blocked
 * standards sites. But GitHub itself is reachable, and this chapter
 * found something better than a schema summary: a real, complete,
 * unmodified OFX 1.02 bank-statement file, posted as a test fixture in
 * the open-source Python library `ofxparse`
 * (github.com/jseutter/ofxparse, `tests/fixtures/checking.ofx`),
 * fetched and read in FULL, the same discipline Chapters 32/34/35
 * applied to NACHA, ACORD, and FIX.
 *
 * OFX 1.x is not XML. It is SGML: a plain "KEY:VALUE" header block,
 * a blank line, then a body of tags where an AGGREGATE element (one
 * that contains other tags) gets an explicit closing tag, but a LEAF
 * element (one that holds only text) does NOT -- it is implicitly
 * closed by whatever tag comes next. This is a real, distinctive
 * structural fact about OFX, confirmed directly in the fetched sample:
 * `<STATUS>` closes explicitly with `</STATUS>`, but its own children
 * `<CODE>0` and `<SEVERITY>INFO` never do; `<STMTTRN>` closes
 * explicitly, but `<TRNTYPE>DEBIT`, `<DTPOSTED>...`, `<TRNAMT>...`,
 * `<FITID>...`, and `<NAME>...` inside it never do. `036_ofx.c`'s own
 * encoder emits real, unclosed leaf tags on the wire, matching that
 * sample exactly; its own decoder reads a leaf by scanning to the next
 * `<` rather than hunting for a matching close tag, and reads an
 * aggregate by scanning to its own explicit `</TAG>` -- which one to
 * do for which tag is fixed by this chapter's own known field list,
 * the same restricted-schema approach 034_acord.c already used for a
 * real but different (fully-closed, XML) format.
 *
 * Every element name below marked [OBSERVED] appears verbatim, in
 * exactly the position this chapter uses it, in that real fetched
 * file: the header block itself (`OFXHEADER`, `DATA`, `VERSION`,
 * `SECURITY`, `ENCODING`, `CHARSET`, `COMPRESSION`, `OLDFILEUID`,
 * `NEWFILEUID`); `OFX`, `SIGNONMSGSRSV1`, `SONRS`, `STATUS`/`CODE`/
 * `SEVERITY`, `DTSERVER`, `LANGUAGE`; `BANKMSGSRSV1`, `STMTTRNRS`,
 * `TRNUID`, `STMTRS`, `CURDEF`, `BANKACCTFROM`/`BANKID`/`ACCTID`/
 * `ACCTTYPE`, `BANKTRANLIST`/`DTSTART`/`DTEND`, `STMTTRN`/`TRNTYPE`/
 * `DTPOSTED`/`TRNAMT`/`FITID`/`NAME`, and `LEDGERBAL`/`BALAMT`/
 * `DTASOF`. The real `TRNTYPE` values this chapter uses, `DEBIT` and
 * `CREDIT`, and the real `ACCTTYPE` value `CHECKING`, and the real
 * `STATUS` values `CODE` 0 / `SEVERITY` `INFO` (success), are likewise
 * all taken directly from that same fetched file, not invented.
 *
 * This chapter's own invention, stated plainly: every fictional bank
 * ID, account ID, transaction amount, date, and merchant NAME/
 * description; the specific budget-category rules and cash-flow
 * forecast model this chapter builds on TOP of a real, correctly
 * parsed OFX feed (see 036_budget.h) are this book's own, not part of
 * OFX itself, which only specifies the transaction feed's own shape.
 *
 * Dates in this fixture (and in this chapter's own messages) are the
 * real OFX `DTSERVER`/`DTPOSTED`/`DTASOF` datetime format, cited from
 * the same fetched file: `YYYYMMDDHHMMSS` (this chapter always uses a
 * fixed `000000` time-of-day, since only the date matters to its own
 * demo). Amounts are plain ASCII decimal, optionally signed, with a
 * '.' before exactly two fractional digits (`-34.51`, `0.01`) -- also
 * cited directly from the fetched file, and converted here to/from
 * this chapter's own internal signed-cents representation. */

#define OFX_MAX_TRANSACTIONS 8u
#define OFX_MAX_NAME_LEN 32u
#define OFX_MAX_MESSAGE_LEN 2048u

typedef struct {
    char trn_type[8];   /* "DEBIT" or "CREDIT" -- real OFX TRNTYPE values */
    char dtposted[9];   /* real OFX date-only prefix, YYYYMMDD + NUL */
    int32_t amount_cents; /* signed: negative for DEBIT, positive for CREDIT */
    char fitid[16];
    char name[OFX_MAX_NAME_LEN];
} ofx_transaction_t;

typedef struct {
    char bank_id[16];
    char acct_id[16];
    char dtstart[9];
    char dtend[9];
    ofx_transaction_t transactions[OFX_MAX_TRANSACTIONS];
    uint32_t transaction_count;
    int32_t ledger_balance_cents;
    char dtasof[9];
} ofx_statement_t;

/* Builds a real-shaped OFX 1.02 bank-statement-download message for
 * `stmt` into `out`. Returns the encoded length, or 0 if `out_size` is
 * too small, `stmt->transaction_count` is 0 or above
 * OFX_MAX_TRANSACTIONS, or any text field is too long for its own
 * stated width. */
uint32_t ofx_build_statement(const ofx_statement_t *stmt, uint8_t *out, uint32_t out_size);

/* Parses exactly `len` bytes. Returns 1 on success, or 0 -- refusing
 * outright -- if the real header block or any required tag is
 * missing, a date is not exactly 8 real digits, an amount is
 * malformed, STATUS/CODE is not "0", or there are more transactions
 * than OFX_MAX_TRANSACTIONS. */
int ofx_parse_statement(const uint8_t *buf, uint32_t len, ofx_statement_t *out_stmt);

#endif
