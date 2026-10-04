/* Chapter 50: a Bitcoin block and transaction validator -- the CONTEXT-FREE rules, the ones that need only the bytes of the block itself (no chain, no coin database, no clock). Bitcoin's consensus is a set of rules every node applies
 * to every block; this chapter implements the first layer of them, from Bitcoin's published formats and rules (the block and transaction serialisation, BIP 34 for the height in the coinbase, BIP 141 for the witness serialisation and the
 * witness commitment, and the checks of Bitcoin Core's CheckBlock/CheckTransaction):
 *   * a block is an 80-byte HEADER (version, previous block hash, Merkle root, time, difficulty bits, nonce) followed by a count and that many transactions;
 *   * its identity is the SHA-256 of the SHA-256 of the header ("double SHA-256"), written in reverse byte order;
 *   * PROOF OF WORK: that hash, read as a 256-bit number, must not exceed the TARGET encoded in the header's "bits" (a compact floating-point form: one exponent byte, three mantissa bytes);
 *   * the MERKLE ROOT in the header must equal the root of the binary hash tree over the transactions' ids (an odd level duplicates its last hash -- which is also how a block with a DUPLICATED transaction can have the right root; Bitcoin
 *     rejects those, and so does this code);
 *   * the first transaction, and only the first, is the COINBASE (the one that creates the new coins);
 *   * every transaction passes its own checks: inputs and outputs present, no negative or over-limit amount (21,000,000 coins is the hard cap), no duplicate input, a coinbase script of 2 to 100 bytes, no null input outside the coinbase;
 *   * if the block carries SegWit witness data, the coinbase commits to the witness Merkle root, and the commitment must match.
 * What this chapter does NOT do, stated here and again on the page: it never runs a script, never checks a signature, never looks up whether an input was already spent, never checks the block reward or the difficulty adjustment against the
 * chain, and never looks at the clock. A block this code calls VALID satisfies the context-free rules; it is not thereby a block the Bitcoin network would accept. */
#ifndef BTC_H
#define BTC_H
#include <stdint.h>

#define BTC_MAX_TX 512
#define BTC_MAX_IN 4096
#define BTC_MAX_MONEY 2100000000000000ll /* 21,000,000 coins in satoshi */
#define BTC_MAX_WEIGHT 4000000u
enum { BTC_OK = 0, BTC_ERR_SHORT = -1, BTC_ERR_TOO_MANY_TX = -2, BTC_ERR_TOO_MANY_IN = -3, BTC_ERR_TRAILING = -4, BTC_ERR_FLAG = -5, BTC_ERR_SUPERFLUOUS_WITNESS = -6, BTC_ERR_VARINT = -7, BTC_ERR_NO_TX = -8 };

typedef struct {
    int32_t version; uint32_t n_in, n_out, lock_time, size, stripped_size, weight; uint8_t has_witness, is_coinbase; int64_t value_out;
    uint8_t txid[32], wtxid[32]; /* raw digest order; shown reversed */
    uint32_t cb_script_off, cb_script_len; /* the coinbase's scriptSig, as an offset into the block bytes */
    uint8_t wit0_items; uint32_t wit0_item0_off, wit0_item0_len; /* the first input's witness: item count and where item 0 is */
    uint8_t commit_found; uint32_t commit_off; /* the last output whose script starts 6a24aa21a9ed: offset of its 32 commitment bytes */
    const char *check; /* NULL when the transaction passes every context-free check, else Bitcoin Core's reason string */
} btc_tx_t;
typedef struct {
    uint32_t version, time, bits, nonce; uint8_t prev[32], merkle[32], hash[32]; uint32_t size, weight, n_tx; btc_tx_t tx[BTC_MAX_TX];
    uint8_t computed_merkle[32]; uint8_t merkle_mutated; uint8_t target_ok; const char *target_why; uint8_t target[32]; /* big-endian, as printed */ uint8_t pow_ok; uint32_t zero_bits;
    uint8_t first_is_cb, other_cb, height_ok; int64_t height; /* BIP 34 height from the coinbase script; height_ok is 0 when there is none */ uint8_t witness; /* 0 none, 1 PASS, 2 FAIL */
    const char *reason; /* NULL = VALID, else the first rule broken */
} btc_block_t;

void btc_dsha(const uint8_t *data, uint32_t len, uint8_t out[32]); /* double SHA-256 */
int btc_tx_parse(const uint8_t *b, uint32_t len, uint32_t pos, btc_tx_t *t, uint32_t *next); /* parses and checks one transaction at b+pos; BTC_OK or an error (the context-free checks never abort parsing: see t->check) */
int btc_block_parse(btc_block_t *B, const uint8_t *b, uint32_t len, uint32_t pow_limit_bits); /* parses and fully checks a block; B->reason is the verdict */
void btc_merkle(const uint8_t (*leaves)[32], uint32_t n, uint8_t root[32], uint8_t *mutated); /* Bitcoin's Merkle root; *mutated set when two equal hashes sit side by side */
int btc_compact(uint32_t bits, uint32_t limit_bits, uint8_t target_be[32], const char **why); /* 1 and the target when valid; 0 and the reason when not */
int btc_report(char *buf, uint32_t cap, const char *name, const btc_block_t *B); /* the canonical text btc_ref.py prints; its length or -1 */
const char *btc_strerror(int rc);
#endif
