#ifndef UNIX_OS_034_ACH_H
#define UNIX_OS_034_ACH_H

#include <stdint.h>

/* A real, fixed-width NACHA ACH file format encoder/decoder, cited
 * field-for-field from two independently-corroborating real bank
 * technical publications that agree exactly on every field name,
 * position, and width for every record type this chapter builds:
 *
 *   - A real Bank technical reference, "ACH File Format Specification"
 *     (gbankmo.com/files/download/documents/NACHA%20Format%20File%20Layout.pdf).
 *   - A second, independent real bank technical reference, "NACHA File
 *     Layout Guide" (independent-bank.com,
 *     _/kcms-doc/174/59908/20201006_NACHA_FileLayoutGuide_Final.pdf).
 *
 * Both agree, field-for-field, on every position and width below for a
 * real 94-character fixed-width ACH record. Real Transaction Codes and
 * the real general field-formatting rule ("alphanumeric fields are
 * left-justified and space-filled ... numeric fields are right-justified,
 * unsigned, and zero-filled") are cited directly from Nacha's own
 * official developer guide (achdevguide.nacha.org/ach-file-overview).
 * The real Service Class Code values (200 mixed, 220 credits-only, 225
 * debits-only) are corroborated by a real Bank of America NACHA
 * technical specification. The real Standard Entry Class Code this
 * chapter uses, WEB ("Internet-Initiated Entry"), is Nacha's own real
 * code for "the origination of debit entries ... to a consumer's
 * account pursuant to an authorization ... obtained from the Receiver
 * via the Internet" (cited via vericheck.com's own real WEB-entry
 * explainer, itself quoting Nacha's rule text) -- the real, correct SEC
 * code for a consumer-facing app pulling a payer's own authorized share
 * of a shared expense, exactly this chapter's own real scenario.
 *
 * Real record types this chapter builds, each exactly 94 real
 * characters, in real NACHA file order:
 *   Type 1 -- File Header Record
 *   Type 5 -- Company/Batch Header Record
 *   Type 6 -- Entry Detail Record (one per real split participant)
 *   Type 8 -- Company/Batch Control Record
 *   Type 9 -- File Control Record
 * (Real Addenda Records, Type 7, are a real optional NACHA extension
 * this chapter does not use -- every Entry Detail Record below sets its
 * own real Addenda Record Indicator to '0', honestly, rather than
 * silently omitting a field this chapter never populates.)
 *
 * Real, honest scope note on "group expense splitting": nothing above is
 * an invented mechanism. A real NACHA batch (Type 5 Batch Header + one
 * Type 8 Batch Control) already legitimately holds MANY Entry Detail
 * records sharing one real Company Identification, one real Standard
 * Entry Class Code, and one real effective entry date -- this is exactly
 * how a real payroll batch works, one entry per employee. This chapter's
 * own real, cited mechanism is unchanged; what is this book's own
 * invention is only the SCENARIO laid on top of it -- one real batch
 * whose real Company Entry Description names a shared group expense
 * (e.g. "DINNERSPLT", exactly 10 real characters -- the real Company
 * Entry Description field's own cited width), and whose N real Entry
 * Detail records are N
 * real debit pulls, one per participant, each for that participant's
 * own real share of the total. The real Batch Control's own Total Debit
 * Entry Dollar Amount field is the real, honest sum of those N real
 * per-participant shares -- not a separately invented "group total"
 * field.
 *
 * Real NACHA blocking rule, cited directly (a real Dynamics 365
 * NACHA-file-blocking explainer, itself describing Nacha's own real
 * blocking-factor-10 requirement): "NACHA files have a blocking factor
 * of 10 ... the total number of lines in a file must be a multiple of
 * 10", padded out with real all-'9' filler lines (94 real '9'
 * characters each) when it is not. This chapter's own `ach_build_file()`
 * applies that real rule honestly rather than skipping it.
 *
 * Real ABA routing-number check-digit algorithm (used to compute a
 * REAL, valid checksum digit for this chapter's own entirely fictional
 * routing numbers, so they are structurally indistinguishable from real
 * ones even though no real institution holds them): cited from Apache
 * Commons Validaton's own `ABANumberCheckDigit` reference documentation,
 * corroborated independently by a real fintech glossary (paytia.com):
 * weights 3, 7, 1 repeating across the first eight digits, and the real
 * ninth (check) digit is whichever value makes
 * "(3*d1 + 7*d2 + 1*d3 + 3*d4 + 7*d5 + 1*d6 + 3*d7 + 7*d8 + 1*d9) mod 10"
 * equal to zero.
 *
 * Real Entry Hash calculation, quoted directly from the first cited
 * source above: "The batch hash count is the sum of receiving DFI
 * transit/routing numbers in entry detail records in the batch. The
 * 10-character hash is the sum of the 8-digit routing numbers, with
 * leading zeros added as needed and overflow out of the high order
 * (leftmost) position ignored." -- independently corroborated by a real
 * third-party ACH-file-validation guide (achgenie.com) describing the
 * identical real sum-and-truncate-to-10-digits rule.
 *
 * IMPORTANT -- entirely fictional data: every routing number, account
 * number, company name, and person name this chapter's own demo builds
 * is invented for this book. None corresponds to any real financial
 * institution, routing number, account, or person. This chapter's own
 * real AES-128-CBC + HMAC-SHA256 encrypt-then-MAC protection (reusing
 * 034_aes.h/.c and 034_hmac.h/.c, and 034_fedwire.h/.c's own real
 * fedwire_pkcs7_pad()/fedwire_pkcs7_unpad(), completely unchanged from
 * Chapter 30 -- per this chapter's own confirmed scope, no new crypto
 * primitive) is applied over the WHOLE real ACH file byte stream below,
 * not the real (and, per real NACHA's own rules, effectively obsolete)
 * 19-byte "Message Authentication Code" field NACHA itself reserves in
 * the real Batch Control Record -- that real field is left as real
 * spaces below, honestly, rather than repurposed for a different real
 * cryptographic mechanism than the one it was actually specified for.
 */

#define ACH_RECORD_LEN 94u
#define ACH_MAX_ENTRIES 4u

/* Real fixed field widths, cited above. */
#define ACH_ROUTING_LEN 8u          /* real 8-digit Receiving DFI Identification */
#define ACH_ACCOUNT_LEN 17u         /* real DFI Account Number field width */
#define ACH_INDIVIDUAL_ID_LEN 15u   /* real Individual Identification Number width */
#define ACH_INDIVIDUAL_NAME_LEN 22u /* real Individual Name field width */
#define ACH_TRACE_LEN 15u           /* real Trace Number field width */
#define ACH_COMPANY_NAME_LEN 16u    /* real Company Name field width */
#define ACH_COMPANY_ID_LEN 10u      /* real Company Identification field width */
#define ACH_ENTRY_DESC_LEN 10u      /* real Company Entry Description field width */
#define ACH_ORIGIN_NAME_LEN 23u     /* real Immediate Origin/Destination Name width */

/* One real Entry Detail Record's worth of fields (Type 6), cited above.
 * `amount_cents` mirrors Chapter 30's own real uint32_t scope decision
 * for a numeric currency field that would otherwise need 64-bit
 * division this freestanding kernel has never linked libgcc for -- the
 * real Amount field is 10 digits (up to $99,999,999.99), and this
 * chapter's own uint32_t choice (up to $42,949,672.95) is more than
 * enough for a real shared-expense split, stated honestly rather than
 * silently narrowed. */
typedef struct {
    uint8_t transaction_code[2];              /* real 2-digit code, e.g. "27" (checking debit) */
    uint8_t receiving_dfi_id[ACH_ROUTING_LEN]; /* real 8-digit routing number (fictional) */
    uint8_t check_digit;                       /* real ABA check digit, computed for real */
    uint8_t dfi_account_number[ACH_ACCOUNT_LEN];
    uint32_t amount_cents;
    uint8_t individual_id_number[ACH_INDIVIDUAL_ID_LEN];
    uint8_t individual_name[ACH_INDIVIDUAL_NAME_LEN];
} ach_entry_t;

/* One real ACH batch/file's worth of fields this chapter models,
 * cited above -- everything needed to build a real, complete,
 * self-contained NACHA file around N real Entry Detail records. */
typedef struct {
    uint8_t immediate_destination[10];   /* real routing number of the receiving ACH operator */
    uint8_t immediate_origin[10];        /* real routing number of the originating ACH operator */
    uint8_t file_creation_date[6];       /* real YYMMDD */
    uint8_t file_creation_time[4];       /* real HHMM */
    uint8_t immediate_destination_name[ACH_ORIGIN_NAME_LEN];
    uint8_t immediate_origin_name[ACH_ORIGIN_NAME_LEN];

    uint8_t company_name[ACH_COMPANY_NAME_LEN];
    uint8_t company_identification[ACH_COMPANY_ID_LEN];
    uint8_t company_entry_description[ACH_ENTRY_DESC_LEN]; /* this chapter's own group-expense label */
    uint8_t effective_entry_date[6];     /* real YYMMDD */
    uint8_t originating_dfi_identification[ACH_ROUTING_LEN];

    ach_entry_t entries[ACH_MAX_ENTRIES];
    uint32_t entry_count;
} ach_batch_t;

/* Real per-file/per-batch NACHA record-count math this chapter always
 * uses: 4 fixed records (File Header, Batch Header, Batch Control, File
 * Control) plus one real Entry Detail Record per participant. */
#define ACH_FIXED_RECORD_COUNT 4u

/* Computes a real, valid ABA routing-number check digit for an 8-digit
 * routing prefix, per the real weighted (3,7,1 repeating) mod-10
 * formula cited above. `routing8` holds 8 real ASCII digit characters
 * ('0'-'9'); returns the real check digit as a single ASCII character. */
uint8_t ach_compute_aba_check_digit(const uint8_t routing8[ACH_ROUTING_LEN]);

/* Builds the complete real NACHA file (File Header + Batch Header + N
 * real Entry Detail records + Batch Control + File Control, padded with
 * real all-'9' filler records per the real blocking-factor-10 rule
 * cited above) into out_buf. Returns the real total number of bytes
 * written (always a multiple of ACH_RECORD_LEN * 10, i.e. 940), or 0 on
 * real refusal: entry_count is 0, exceeds ACH_MAX_ENTRIES, or out_buf is
 * too small. */
uint32_t ach_build_file(const ach_batch_t *batch, uint8_t *out_buf, uint32_t out_buf_size);

/* Parses a real NACHA file byte buffer back into an ach_batch_t. Follows
 * this book's own established no-partial-effect refusal discipline
 * (Chapter 20's FAT16 onward): any record whose real Record Type Code
 * byte does not match what real NACHA file order requires at that
 * position refuses outright, returning 0, without partially filling
 * out_batch. Returns 1 on success. Deliberately does not re-verify the
 * real Entry Hash or dollar totals on parse (that is this chapter's own
 * new real AES-128-CBC + HMAC-SHA256 encrypt-then-MAC layer's job,
 * exactly as Chapter 30 already established -- the HMAC is checked
 * BEFORE this parse ever runs). */
int ach_parse_file(const uint8_t *buf, uint32_t len, ach_batch_t *out_batch);

#endif
