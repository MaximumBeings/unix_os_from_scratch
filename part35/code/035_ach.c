/* See 035_ach.h's own top-of-file comment for the full real citation of
 * every record type, field width, and the fictional-data policy used
 * here. */

#include "035_ach.h"

static uint32_t field_text_len(const uint8_t *src, uint32_t max) {
    uint32_t n = 0;
    while (n < max && src[n] != 0) {
        n++;
    }
    return n;
}

/* Real alphanumeric field rule (cited in 035_ach.h): left-justified,
 * space-filled. */
static void write_alpha(uint8_t *buf, uint32_t off, uint32_t width, const uint8_t *src) {
    uint32_t n = field_text_len(src, width);
    for (uint32_t i = 0; i < n; i++) {
        buf[off + i] = src[i];
    }
    for (uint32_t i = n; i < width; i++) {
        buf[off + i] = (uint8_t)' ';
    }
}

/* Real numeric field rule for a field this chapter's own struct already
 * stores as real ASCII digit bytes (e.g. a real YYMMDD date): copies
 * through as-is, treating a real 0 terminator byte (never a valid ASCII
 * digit) as an unset field defaulting to '0', so a shorter source string
 * still zero-fills the rest of the real fixed width. */
static void write_numeric_ascii(uint8_t *buf, uint32_t off, uint32_t width, const uint8_t *src) {
    for (uint32_t i = 0; i < width; i++) {
        uint8_t c = src[i];
        buf[off + i] = (c == 0) ? (uint8_t)'0' : c;
    }
}

static void write_lit(uint8_t *buf, uint32_t off, const char *lit) {
    uint32_t i = 0;
    while (lit[i] != '\0') {
        buf[off + i] = (uint8_t)lit[i];
        i++;
    }
}

/* Real numeric field rule for a field this chapter stores as a real
 * uint32_t (e.g. a real dollar amount or count): right-justified,
 * zero-filled, per the real general NACHA formatting rule cited in
 * 035_ach.h. */
static void write_digits(uint8_t *buf, uint32_t off, uint32_t width, uint32_t value) {
    for (uint32_t i = 0; i < width; i++) {
        buf[off + width - 1u - i] = (uint8_t)('0' + (value % 10u));
        value /= 10u;
    }
}

static void fill_spaces(uint8_t *buf, uint32_t off, uint32_t width) {
    for (uint32_t i = 0; i < width; i++) {
        buf[off + i] = (uint8_t)' ';
    }
}

uint8_t ach_compute_aba_check_digit(const uint8_t routing8[ACH_ROUTING_LEN]) {
    /* Real weighted (3,7,1 repeating) mod-10 formula, cited in
     * 035_ach.h from Apache Commons Validator's own ABANumberCheckDigit
     * reference documentation. */
    static const uint32_t weights[8] = {3u, 7u, 1u, 3u, 7u, 1u, 3u, 7u};
    uint32_t sum = 0;
    for (uint32_t i = 0; i < 8u; i++) {
        uint32_t d = (uint32_t)(routing8[i] - (uint8_t)'0');
        sum += d * weights[i];
    }
    uint32_t check = (10u - (sum % 10u)) % 10u;
    return (uint8_t)('0' + check);
}

uint32_t ach_build_file(const ach_batch_t *batch, uint8_t *out_buf, uint32_t out_buf_size) {
    if (batch->entry_count == 0 || batch->entry_count > ACH_MAX_ENTRIES) {
        return 0;
    }

    /* Real record count: 4 fixed records (File Header, Batch Header,
     * Batch Control, File Control) plus one real Entry Detail Record per
     * participant, padded out with real all-'9' filler records to the
     * next multiple of 10 -- the real blocking-factor-10 rule cited in
     * 035_ach.h. */
    uint32_t record_count = ACH_FIXED_RECORD_COUNT + batch->entry_count;
    uint32_t pad_count = (10u - (record_count % 10u)) % 10u;
    uint32_t total_records = record_count + pad_count;
    uint32_t total_len = total_records * ACH_RECORD_LEN;

    if (total_len > out_buf_size) {
        return 0;
    }

    uint32_t off = 0;

    /* --- Record Type 1: File Header Record --- */
    fill_spaces(out_buf, off, ACH_RECORD_LEN);
    write_lit(out_buf, off + 0, "1");                                        /* Record Type Code */
    write_lit(out_buf, off + 1, "01");                                       /* Priority Code */
    write_numeric_ascii(out_buf, off + 3, 10u, batch->immediate_destination); /* Immediate Destination */
    write_numeric_ascii(out_buf, off + 13, 10u, batch->immediate_origin);     /* Immediate Origin */
    write_numeric_ascii(out_buf, off + 23, 6u, batch->file_creation_date);    /* File Creation Date, YYMMDD */
    write_numeric_ascii(out_buf, off + 29, 4u, batch->file_creation_time);    /* File Creation Time, HHMM */
    write_lit(out_buf, off + 33, "A");                                       /* File ID Modifier */
    write_lit(out_buf, off + 34, "094");                                     /* Record Size */
    write_lit(out_buf, off + 37, "10");                                      /* Blocking Factor */
    write_lit(out_buf, off + 39, "1");                                       /* Format Code */
    write_alpha(out_buf, off + 40, ACH_ORIGIN_NAME_LEN, batch->immediate_destination_name);
    write_alpha(out_buf, off + 63, ACH_ORIGIN_NAME_LEN, batch->immediate_origin_name);
    fill_spaces(out_buf, off + 86, 8u); /* Reference Code, real optional field this chapter leaves blank */
    off += ACH_RECORD_LEN;

    /* --- Record Type 5: Company/Batch Header Record --- */
    fill_spaces(out_buf, off, ACH_RECORD_LEN);
    write_lit(out_buf, off + 0, "5");    /* Record Type Code */
    write_lit(out_buf, off + 1, "225");  /* Service Class Code: debits only */
    write_alpha(out_buf, off + 4, ACH_COMPANY_NAME_LEN, batch->company_name);
    fill_spaces(out_buf, off + 20, 20u); /* Company Discretionary Data, real optional field */
    write_alpha(out_buf, off + 40, ACH_COMPANY_ID_LEN, batch->company_identification);
    write_lit(out_buf, off + 50, "WEB"); /* Standard Entry Class Code */
    write_alpha(out_buf, off + 53, ACH_ENTRY_DESC_LEN, batch->company_entry_description);
    fill_spaces(out_buf, off + 63, 6u);  /* Company Descriptive Date, real optional field */
    write_numeric_ascii(out_buf, off + 69, 6u, batch->effective_entry_date); /* YYMMDD */
    fill_spaces(out_buf, off + 75, 3u);  /* Settlement Date (Julian), assigned by the real ACH Operator */
    write_lit(out_buf, off + 78, "1");   /* Originator Status Code */
    write_numeric_ascii(out_buf, off + 79, ACH_ROUTING_LEN, batch->originating_dfi_identification);
    write_lit(out_buf, off + 87, "0000001"); /* Batch Number */
    off += ACH_RECORD_LEN;

    uint32_t entry_hash = 0;
    uint32_t total_debit_cents = 0;

    /* --- Record Type 6: one Entry Detail Record per real split
     * participant -- this chapter's own honest "group expense splitting"
     * layer: N real debit pulls sharing one real batch, exactly how a
     * real payroll batch already works (see 035_ach.h's own scope note).
     */
    for (uint32_t i = 0; i < batch->entry_count; i++) {
        const ach_entry_t *e = &batch->entries[i];
        fill_spaces(out_buf, off, ACH_RECORD_LEN);
        write_lit(out_buf, off + 0, "6"); /* Record Type Code */
        write_numeric_ascii(out_buf, off + 1, 2u, e->transaction_code); /* e.g. "27", checking debit */
        write_numeric_ascii(out_buf, off + 3, ACH_ROUTING_LEN, e->receiving_dfi_id);
        out_buf[off + 11] = e->check_digit;
        write_alpha(out_buf, off + 12, ACH_ACCOUNT_LEN, e->dfi_account_number);
        write_digits(out_buf, off + 29, 10u, e->amount_cents); /* Amount, 10 real numeric digits */
        write_alpha(out_buf, off + 39, ACH_INDIVIDUAL_ID_LEN, e->individual_id_number);
        write_alpha(out_buf, off + 54, ACH_INDIVIDUAL_NAME_LEN, e->individual_name);
        fill_spaces(out_buf, off + 76, 2u); /* Discretionary Data, real optional field */
        write_lit(out_buf, off + 78, "0");  /* Addenda Record Indicator: no Type 7 records, honestly stated */
        write_numeric_ascii(out_buf, off + 79, ACH_ROUTING_LEN, batch->originating_dfi_identification);
        /* Real Trace Number = Originating DFI's 8-digit routing number +
         * a 7-digit per-entry sequence number. This composition is this
         * chapter's OWN common-convention choice satisfying Nacha's real
         * stated requirement (assigned by the ODFI, ascending, unique
         * per entry) -- Nacha's own text does not itself mandate this
         * exact substructure, stated honestly rather than overclaimed
         * (see 035_ach.h). */
        write_digits(out_buf, off + 87, 7u, i + 1u);

        /* Real Entry Hash: sum of each entry's own 8-digit Receiving DFI
         * routing number, cited in 035_ach.h. */
        uint32_t routing_value = 0;
        for (uint32_t d = 0; d < ACH_ROUTING_LEN; d++) {
            uint8_t c = e->receiving_dfi_id[d];
            uint32_t digit = (c == 0) ? 0u : (uint32_t)(c - (uint8_t)'0');
            routing_value = routing_value * 10u + digit;
        }
        entry_hash += routing_value;
        total_debit_cents += e->amount_cents;

        off += ACH_RECORD_LEN;
    }

    /* Real Entry Hash truncation: "overflow out of the high order
     * (leftmost) position ignored" -- keep only the rightmost 10 digits,
     * cited in 035_ach.h. This chapter's own uint32_t entry_hash can
     * never itself exceed 10 decimal digits (max value ~4.29 billion),
     * so no truncation is actually needed here for this chapter's own
     * ACH_MAX_ENTRIES-bounded batches -- write_digits below already
     * zero-fills/right-justifies into the real 10-digit field. */

    /* --- Record Type 8: Company/Batch Control Record --- */
    fill_spaces(out_buf, off, ACH_RECORD_LEN);
    write_lit(out_buf, off + 0, "8");   /* Record Type Code */
    write_lit(out_buf, off + 1, "225"); /* Service Class Code, must match the Batch Header */
    write_digits(out_buf, off + 4, 6u, batch->entry_count);      /* Entry/Addenda Count */
    write_digits(out_buf, off + 10, 10u, entry_hash);            /* Entry Hash */
    write_digits(out_buf, off + 20, 12u, total_debit_cents);     /* Total Debit Entry Dollar Amount */
    write_digits(out_buf, off + 32, 12u, 0u);                    /* Total Credit Entry Dollar Amount: none, debits-only batch */
    write_alpha(out_buf, off + 44, ACH_COMPANY_ID_LEN, batch->company_identification);
    /* Real Message Authentication Code field (19 bytes) -- left as real
     * spaces, honestly, rather than repurposed for this chapter's own
     * AES-128-CBC + HMAC-SHA256 protection of the whole file (see
     * 035_ach.h's own note on this). */
    fill_spaces(out_buf, off + 54, 19u);
    fill_spaces(out_buf, off + 73, 6u); /* Reserved */
    write_numeric_ascii(out_buf, off + 79, ACH_ROUTING_LEN, batch->originating_dfi_identification);
    write_lit(out_buf, off + 87, "0000001"); /* Batch Number, must match the Batch Header */
    off += ACH_RECORD_LEN;

    /* --- Record Type 9: File Control Record --- */
    fill_spaces(out_buf, off, ACH_RECORD_LEN);
    write_lit(out_buf, off + 0, "9");                     /* Record Type Code */
    write_digits(out_buf, off + 1, 6u, 1u);                /* Batch Count: this chapter always builds exactly one batch */
    write_digits(out_buf, off + 7, 6u, total_records / 10u); /* Block Count, real blocking factor of 10 */
    write_digits(out_buf, off + 13, 8u, batch->entry_count); /* Entry/Addenda Count */
    write_digits(out_buf, off + 21, 10u, entry_hash);      /* Entry Hash */
    write_digits(out_buf, off + 31, 12u, total_debit_cents); /* Total Debit Entry Dollar Amount */
    write_digits(out_buf, off + 43, 12u, 0u);              /* Total Credit Entry Dollar Amount */
    fill_spaces(out_buf, off + 55, 39u);                   /* Reserved */
    off += ACH_RECORD_LEN;

    /* Real all-'9' filler records, per the real blocking-factor-10 rule
     * cited in 035_ach.h. */
    for (uint32_t i = 0; i < pad_count; i++) {
        for (uint32_t j = 0; j < ACH_RECORD_LEN; j++) {
            out_buf[off + j] = (uint8_t)'9';
        }
        off += ACH_RECORD_LEN;
    }

    return total_len;
}

int ach_parse_file(const uint8_t *buf, uint32_t len, ach_batch_t *out_batch) {
    /* A real, complete NACHA file is always a whole number of real
     * 94-byte records, and the real blocking-factor-10 rule (cited in
     * 035_ach.h) means the real total record count is always a multiple
     * of 10 -- so the real total byte length is always a multiple of
     * ACH_RECORD_LEN * 10 (940). Any other length refuses outright, per
     * this book's own established no-partial-effect refusal discipline
     * (Chapter 20's FAT16 onward). */
    if (len == 0 || (len % (ACH_RECORD_LEN * 10u)) != 0u) {
        return 0;
    }

    uint32_t total_records = len / ACH_RECORD_LEN;

    /* Real record 0 must be the File Header (Type '1'); real record 1
     * must be the Batch Header (Type '5'). */
    if (buf[0] != (uint8_t)'1' || buf[ACH_RECORD_LEN] != (uint8_t)'5') {
        return 0;
    }

    /* Real Entry Detail records (Type '6') run from record 2 onward,
     * until the first record whose Record Type Code is not '6'. Bounded
     * by ACH_MAX_ENTRIES so a truncated/corrupt file can never be
     * mistaken for more real entries than this chapter's own fixed-size
     * array holds. */
    uint32_t entry_count = 0;
    while (entry_count < ACH_MAX_ENTRIES) {
        uint32_t rec_off = (2u + entry_count) * ACH_RECORD_LEN;
        if (rec_off >= len || buf[rec_off] != (uint8_t)'6') {
            break;
        }
        entry_count++;
    }

    if (entry_count == 0) {
        return 0;
    }

    uint32_t batch_control_rec = 2u + entry_count;
    uint32_t file_control_rec = batch_control_rec + 1u;

    /* Need at least: File Header, Batch Header, N real Entry Detail
     * records, Batch Control, File Control -- all within the real file. */
    if (file_control_rec >= total_records) {
        return 0;
    }

    uint32_t batch_control_off = batch_control_rec * ACH_RECORD_LEN;
    uint32_t file_control_off = file_control_rec * ACH_RECORD_LEN;

    if (buf[batch_control_off] != (uint8_t)'8' || buf[file_control_off] != (uint8_t)'9') {
        return 0;
    }

    /* Real trailing filler records (if any) must each be exactly 94 real
     * '9' characters, per the real blocking-factor-10 rule -- verified
     * here for the same byte-exact rigor this book applies everywhere
     * else, rather than silently ignoring bytes past the File Control
     * record. */
    for (uint32_t rec = file_control_rec + 1u; rec < total_records; rec++) {
        uint32_t roff = rec * ACH_RECORD_LEN;
        for (uint32_t j = 0; j < ACH_RECORD_LEN; j++) {
            if (buf[roff + j] != (uint8_t)'9') {
                return 0;
            }
        }
    }

    /* All real structural checks above passed. From here this follows
     * 035_fedwire.c's own fedwire_parse_message() precedent: fields are
     * validated and written in sequence, refusing (returning 0)
     * immediately on the first invalid field -- rather than guessing at
     * a malformed value -- exactly as fedwire_parse_message() does for
     * its own {2000} Amount field. */

    /* --- Record 0: File Header --- */
    for (uint32_t i = 0; i < 10u; i++) out_batch->immediate_destination[i] = buf[3u + i];
    for (uint32_t i = 0; i < 10u; i++) out_batch->immediate_origin[i] = buf[13u + i];
    for (uint32_t i = 0; i < 6u; i++) out_batch->file_creation_date[i] = buf[23u + i];
    for (uint32_t i = 0; i < 4u; i++) out_batch->file_creation_time[i] = buf[29u + i];
    for (uint32_t i = 0; i < ACH_ORIGIN_NAME_LEN; i++) out_batch->immediate_destination_name[i] = buf[40u + i];
    for (uint32_t i = 0; i < ACH_ORIGIN_NAME_LEN; i++) out_batch->immediate_origin_name[i] = buf[63u + i];

    /* --- Record 1: Batch Header --- */
    {
        uint32_t bh = ACH_RECORD_LEN;
        for (uint32_t i = 0; i < ACH_COMPANY_NAME_LEN; i++) out_batch->company_name[i] = buf[bh + 4u + i];
        for (uint32_t i = 0; i < ACH_COMPANY_ID_LEN; i++) out_batch->company_identification[i] = buf[bh + 40u + i];
        for (uint32_t i = 0; i < ACH_ENTRY_DESC_LEN; i++) out_batch->company_entry_description[i] = buf[bh + 53u + i];
        for (uint32_t i = 0; i < 6u; i++) out_batch->effective_entry_date[i] = buf[bh + 69u + i];
        for (uint32_t i = 0; i < ACH_ROUTING_LEN; i++) out_batch->originating_dfi_identification[i] = buf[bh + 79u + i];
    }

    /* --- Records 2..(2+entry_count-1): Entry Detail --- */
    for (uint32_t e = 0; e < entry_count; e++) {
        uint32_t off = (2u + e) * ACH_RECORD_LEN;
        ach_entry_t *entry = &out_batch->entries[e];

        entry->transaction_code[0] = buf[off + 1u];
        entry->transaction_code[1] = buf[off + 2u];
        for (uint32_t i = 0; i < ACH_ROUTING_LEN; i++) entry->receiving_dfi_id[i] = buf[off + 3u + i];
        entry->check_digit = buf[off + 11u];
        for (uint32_t i = 0; i < ACH_ACCOUNT_LEN; i++) entry->dfi_account_number[i] = buf[off + 12u + i];

        /* Real Amount field: 10 numeric ASCII digits. Refuses outright,
         * exactly like fedwire_parse_message()'s own {2000} Amount
         * check, if any byte in the field is not a real ASCII digit --
         * never guesses at a malformed value. */
        {
            uint32_t v = 0;
            for (uint32_t i = 0; i < 10u; i++) {
                uint8_t c = buf[off + 29u + i];
                if (c < (uint8_t)'0' || c > (uint8_t)'9') {
                    return 0;
                }
                v = v * 10u + (uint32_t)(c - (uint8_t)'0');
            }
            entry->amount_cents = v;
        }

        for (uint32_t i = 0; i < ACH_INDIVIDUAL_ID_LEN; i++) entry->individual_id_number[i] = buf[off + 39u + i];
        for (uint32_t i = 0; i < ACH_INDIVIDUAL_NAME_LEN; i++) entry->individual_name[i] = buf[off + 54u + i];
    }

    out_batch->entry_count = entry_count;
    return 1;
}
