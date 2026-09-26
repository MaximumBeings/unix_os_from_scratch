/* See 035_acord.h's own top-of-file comment for the full citation trail
 * (OBSERVED elements read directly from a real, cloned GitHub sample;
 * WEAK elements known only via web-search summaries; INVENTED elements
 * this book made up outright) and this module's own stated XML subset.
 *
 * One more citation note, specific to this file: the CoverageCd values
 * used below, `BI` (Bodily Injury), `PD` (Property Damage), and `COLL`
 * (Collision), are common real US auto-insurance abbreviations, but
 * were NOT observed in the cited Workers'-Compensation sample (whose
 * own CoverageCd values -- `FORGN`, `VOL`, `WCEL` -- are workers'-comp
 * specific and not reusable here). They are marked [WEAK] like the
 * other personal-auto-specific names in 035_acord.h: real per general
 * industry knowledge, not verified against a document this book read
 * in full. */

#include "035_acord.h"

/* ---------------------------------------------------------------- */
/* A minimal literal-substring XML scanner, matching only this file's */
/* own stated subset (see 035_acord.h): no attributes, no whitespace  */
/* between elements, no same-name self-nesting.                      */
/* ---------------------------------------------------------------- */

/* This chapter uses element names up to 24 bytes long
 * ("PersAutoPolicyQuoteInqRq"/"...Rs", 24 characters each). A wrapped
 * tag needs strlen(tag) + 4 bytes ("<", an optional "/", ">", and a
 * NUL), so every scratch buffer below is sized off this dedicated
 * constant -- not off 035_acord.h's own ACORD_MAX_NAME_LEN, which
 * sizes the unrelated surname/given-name fields and, at exactly 24, is
 * one coincidental digit short of these tag names' own length: an
 * earlier version of this file reused ACORD_MAX_NAME_LEN + 3 for both
 * purposes, which overflowed a stack buffer by exactly one byte on
 * every closing tag of this length and crashed the kernel with a page
 * fault -- caught only by booting it, not by the native test (whose
 * own stack layout happened not to corrupt anything the crash would
 * show). */
#define ACORD_MAX_TAG_LEN 32u

static uint32_t str_len(const char *s) {
    uint32_t n = 0;
    while (s[n] != '\0') {
        n++;
    }
    return n;
}

/* True if `buf[pos..)` begins with the literal bytes of `s` and still
 * fits before `end`. */
static int matches_at(const uint8_t *buf, uint32_t pos, uint32_t end, const char *s) {
    uint32_t n = str_len(s);
    if (pos + n > end) {
        return 0;
    }
    for (uint32_t i = 0; i < n; i++) {
        if (buf[pos + i] != (uint8_t)s[i]) {
            return 0;
        }
    }
    return 1;
}

/* Builds "<tag>" (or "</tag>" with `closing`) into a small stack
 * buffer and returns its length; `tag` is always a short literal, so
 * ACORD_MAX_TAG_LEN + 4 bytes is always enough (this file's own dedicated tag-name-sized constant, not 035_acord.h's ACORD_MAX_NAME_LEN, which sizes the unrelated surname/given-name fields -- see the fix note above make_wrapped_tag()). */
static uint32_t make_wrapped_tag(char *scratch, const char *tag, int closing) {
    uint32_t n = 0;
    scratch[n++] = '<';
    if (closing) {
        scratch[n++] = '/';
    }
    uint32_t tn = str_len(tag);
    for (uint32_t i = 0; i < tn; i++) {
        scratch[n++] = tag[i];
    }
    scratch[n++] = '>';
    scratch[n] = '\0';
    return n;
}

/* Searches [start, end) for the literal opening tag "<tag>", then for
 * the first "</tag>" after it. On success, returns 1, sets
 * *out_content_start / *out_content_end to the exact span strictly
 * between them, and *out_after to the position right after the closing
 * tag's own '>'. Returns 0 -- refusing outright -- if the opening tag
 * is not found before `end`, or its matching closing tag is not found
 * before `end` (a malformed or truncated message; this restricted
 * grammar never needs to look further, since `tag` never nests inside
 * itself). */
static int acord_find(const uint8_t *buf, uint32_t start, uint32_t end, const char *tag,
                      uint32_t *out_content_start, uint32_t *out_content_end, uint32_t *out_after) {
    char open_tag[ACORD_MAX_TAG_LEN + 4];
    char close_tag[ACORD_MAX_TAG_LEN + 4];
    uint32_t open_len = make_wrapped_tag(open_tag, tag, 0);
    uint32_t close_len = make_wrapped_tag(close_tag, tag, 1);

    uint32_t pos = start;
    int found_open = 0;
    for (; pos + open_len <= end; pos++) {
        if (matches_at(buf, pos, end, open_tag)) {
            found_open = 1;
            break;
        }
    }
    if (!found_open) {
        return 0;
    }
    uint32_t content_start = pos + open_len;

    int found_close = 0;
    uint32_t cpos = content_start;
    for (; cpos + close_len <= end; cpos++) {
        if (matches_at(buf, cpos, end, close_tag)) {
            found_close = 1;
            break;
        }
    }
    if (!found_close) {
        return 0;
    }
    *out_content_start = content_start;
    *out_content_end = cpos;
    *out_after = cpos + close_len;
    return 1;
}

static int is_digit(uint8_t c) {
    return c >= (uint8_t)'0' && c <= (uint8_t)'9';
}

/* Parses ASCII decimal digits in [start, end) into *out. Refuses
 * (returns 0) on an empty span, a non-digit byte, or a value too large
 * for a uint32_t. */
static int parse_uint_span(const uint8_t *buf, uint32_t start, uint32_t end, uint32_t *out) {
    if (start >= end) {
        return 0;
    }
    uint32_t v = 0;
    for (uint32_t i = start; i < end; i++) {
        if (!is_digit(buf[i])) {
            return 0;
        }
        uint32_t d = (uint32_t)(buf[i] - (uint8_t)'0');
        if (v > (0xFFFFFFFFu - d) / 10u) {
            return 0;
        }
        v = v * 10u + d;
    }
    *out = v;
    return 1;
}

/* Copies buf[start,end) into `dst` (dst_size bytes) and NUL-terminates
 * it. Refuses (returns 0) if the span, plus the NUL, would not fit. */
static int copy_text_span(const uint8_t *buf, uint32_t start, uint32_t end, char *dst,
                          uint32_t dst_size) {
    uint32_t n = end - start;
    if (n + 1u > dst_size) {
        return 0;
    }
    for (uint32_t i = 0; i < n; i++) {
        dst[i] = (char)buf[start + i];
    }
    dst[n] = '\0';
    return 1;
}

/* ---------------------------------------------------------------- */
/* Encoding                                                         */
/* ---------------------------------------------------------------- */

/* Appends `s` (a NUL-terminated string of known-safe XML text: this
 * chapter's own fictional names, state codes, and decimal digits, none
 * of which contain '<', '>', or '&') into `buf`, refusing (returning 0)
 * if it would not fit in `size`. */
static int append_str(uint8_t *buf, uint32_t *pos, uint32_t size, const char *s) {
    uint32_t n = str_len(s);
    if (*pos + n > size) {
        return 0;
    }
    for (uint32_t i = 0; i < n; i++) {
        buf[*pos + i] = (uint8_t)s[i];
    }
    *pos += n;
    return 1;
}

static int append_open(uint8_t *buf, uint32_t *pos, uint32_t size, const char *tag) {
    char scratch[ACORD_MAX_TAG_LEN + 4];
    make_wrapped_tag(scratch, tag, 0);
    return append_str(buf, pos, size, scratch);
}

static int append_close(uint8_t *buf, uint32_t *pos, uint32_t size, const char *tag) {
    char scratch[ACORD_MAX_TAG_LEN + 4];
    make_wrapped_tag(scratch, tag, 1);
    return append_str(buf, pos, size, scratch);
}

static int append_leaf(uint8_t *buf, uint32_t *pos, uint32_t size, const char *tag,
                       const char *text) {
    return append_open(buf, pos, size, tag) && append_str(buf, pos, size, text) &&
           append_close(buf, pos, size, tag);
}

/* Converts `value` to decimal ASCII (no leading zeros; "0" for zero)
 * and appends it as a <tag>...</tag> leaf. A uint32_t is at most 10
 * digits. */
static int append_uint_leaf(uint8_t *buf, uint32_t *pos, uint32_t size, const char *tag,
                           uint32_t value) {
    char digits[11];
    uint32_t n = 0;
    if (value == 0u) {
        digits[n++] = '0';
    } else {
        char rev[10];
        uint32_t rn = 0;
        while (value > 0u) {
            rev[rn++] = (char)('0' + (value % 10u));
            value /= 10u; /* a plain uint32_t modulo/division: 32-bit, no libgcc call */
        }
        while (rn > 0u) {
            digits[n++] = rev[--rn];
        }
    }
    digits[n] = '\0';
    return append_leaf(buf, pos, size, tag, digits);
}

/* FormatCurrencyAmt > Amt -- the real OBSERVED pattern (035_acord.h),
 * reused for every dollar amount in this message. The caller opens and
 * closes its own wrapper (Limit/Deductible/PolicyAmt/VehCurrentValue)
 * around this. */
static int append_amount(uint8_t *buf, uint32_t *pos, uint32_t size, uint32_t cents) {
    return append_open(buf, pos, size, "FormatCurrencyAmt") &&
           append_uint_leaf(buf, pos, size, "Amt", cents) &&
           append_close(buf, pos, size, "FormatCurrencyAmt");
}

static int append_limit(uint8_t *buf, uint32_t *pos, uint32_t size, uint32_t cents,
                        const char *applies_to_cd) {
    int ok = append_open(buf, pos, size, "Limit") && append_amount(buf, pos, size, cents);
    if (applies_to_cd != 0) {
        ok = ok && append_leaf(buf, pos, size, "LimitAppliesToCd", applies_to_cd);
    }
    return ok && append_close(buf, pos, size, "Limit");
}

static int append_deductible(uint8_t *buf, uint32_t *pos, uint32_t size, uint32_t cents) {
    return append_open(buf, pos, size, "Deductible") && append_amount(buf, pos, size, cents) &&
           append_close(buf, pos, size, "Deductible");
}

uint32_t acord_build_request(const acord_request_t *r, uint8_t *out, uint32_t out_size) {
    uint32_t pos = 0;
    int ok = 1;

    ok = ok && append_open(out, &pos, out_size, "ACORD");
    ok = ok && append_open(out, &pos, out_size, "SignonRq");
    ok = ok && append_leaf(out, &pos, out_size, "CustLoginId", "SPLITJOY-INS");
    ok = ok && append_leaf(out, &pos, out_size, "ClientDt", r->effective_date);
    ok = ok && append_open(out, &pos, out_size, "ClientApp");
    ok = ok && append_leaf(out, &pos, out_size, "Org", "SplitJoy Insurance Comparison");
    ok = ok && append_leaf(out, &pos, out_size, "Name", "SplitJoy Compare");
    ok = ok && append_close(out, &pos, out_size, "ClientApp");
    ok = ok && append_close(out, &pos, out_size, "SignonRq");

    ok = ok && append_open(out, &pos, out_size, "InsuranceSvcRq");
    ok = ok && append_open(out, &pos, out_size, "ItemIdInfo");
    ok = ok && append_open(out, &pos, out_size, "OtherIdentifier");
    ok = ok && append_leaf(out, &pos, out_size, "OtherIdTypeCd", "Request");
    ok = ok && append_leaf(out, &pos, out_size, "UID", "033-CH34-DEMO-0001");
    ok = ok && append_close(out, &pos, out_size, "OtherIdentifier");
    ok = ok && append_close(out, &pos, out_size, "ItemIdInfo");

    ok = ok && append_open(out, &pos, out_size, "PersAutoPolicyQuoteInqRq");
    ok = ok && append_leaf(out, &pos, out_size, "TransactionRequestDt", r->effective_date);
    ok = ok && append_leaf(out, &pos, out_size, "CurCd", "USD");

    ok = ok && append_open(out, &pos, out_size, "InsuredOrPrincipal");
    ok = ok && append_open(out, &pos, out_size, "GeneralPartyInfo");
    ok = ok && append_open(out, &pos, out_size, "NameInfo");
    ok = ok && append_open(out, &pos, out_size, "PersonName");
    ok = ok && append_leaf(out, &pos, out_size, "Surname", r->surname);
    ok = ok && append_leaf(out, &pos, out_size, "GivenName", r->given_name);
    ok = ok && append_close(out, &pos, out_size, "PersonName");
    ok = ok && append_close(out, &pos, out_size, "NameInfo");
    ok = ok && append_open(out, &pos, out_size, "Addr");
    ok = ok && append_leaf(out, &pos, out_size, "StateProvCd", r->state_prov_cd);
    ok = ok && append_leaf(out, &pos, out_size, "PostalCode", r->postal_code);
    ok = ok && append_close(out, &pos, out_size, "Addr");
    ok = ok && append_close(out, &pos, out_size, "GeneralPartyInfo");

    ok = ok && append_open(out, &pos, out_size, "PersDriverInfo");
    ok = ok && append_uint_leaf(out, &pos, out_size, "Age", r->applicant.driver_age);
    ok = ok && append_uint_leaf(out, &pos, out_size, "YearsLicensed", r->applicant.years_licensed);
    ok = ok && append_uint_leaf(out, &pos, out_size, "AtFaultAccidentCnt",
                               r->applicant.at_fault_accidents_3yr);
    ok = ok && append_close(out, &pos, out_size, "PersDriverInfo");

    ok = ok && append_open(out, &pos, out_size, "PersVehInfo");
    ok = ok && append_open(out, &pos, out_size, "VehCurrentValue");
    ok = ok && append_amount(out, &pos, out_size, r->applicant.vehicle_value_cents);
    ok = ok && append_close(out, &pos, out_size, "VehCurrentValue");
    ok = ok && append_uint_leaf(out, &pos, out_size, "VehAge", r->applicant.vehicle_age_years);
    ok = ok && append_close(out, &pos, out_size, "PersVehInfo");
    ok = ok && append_close(out, &pos, out_size, "InsuredOrPrincipal");

    /* This chapter's own invented [WEAK/INVENTED] TerritoryTierCd leaf
     * -- 035_insurance.c's own rating table needs the applicant's
     * territory tier, and this chapter found no real ACORD element for
     * it under this sandbox's network policy, so it is carried as a
     * plain invented tag rather than folded silently into Addr. */
    ok = ok && append_uint_leaf(out, &pos, out_size, "TerritoryTierCd", r->applicant.territory_tier);

    ok = ok && append_open(out, &pos, out_size, "Policy");
    ok = ok && append_leaf(out, &pos, out_size, "LOBCd", "AUTOP");
    ok = ok && append_open(out, &pos, out_size, "ContractTerm");
    ok = ok && append_leaf(out, &pos, out_size, "EffectiveDt", r->effective_date);
    ok = ok && append_leaf(out, &pos, out_size, "ExpirationDt", r->expiration_date);
    ok = ok && append_close(out, &pos, out_size, "ContractTerm");

    ok = ok && append_open(out, &pos, out_size, "Coverage");
    ok = ok && append_leaf(out, &pos, out_size, "CoverageCd", "BI");
    ok = ok && append_limit(out, &pos, out_size, r->applicant.bi_per_person_cents, "PerPerson");
    ok = ok && append_limit(out, &pos, out_size, r->applicant.bi_per_accident_cents, "PerAcc");
    ok = ok && append_close(out, &pos, out_size, "Coverage");

    ok = ok && append_open(out, &pos, out_size, "Coverage");
    ok = ok && append_leaf(out, &pos, out_size, "CoverageCd", "PD");
    ok = ok && append_limit(out, &pos, out_size, r->applicant.pd_cents, 0);
    ok = ok && append_close(out, &pos, out_size, "Coverage");

    ok = ok && append_open(out, &pos, out_size, "Coverage");
    ok = ok && append_leaf(out, &pos, out_size, "CoverageCd", "COLL");
    ok = ok && append_deductible(out, &pos, out_size, r->applicant.collision_deductible_cents);
    ok = ok && append_close(out, &pos, out_size, "Coverage");

    ok = ok && append_close(out, &pos, out_size, "Policy");
    ok = ok && append_close(out, &pos, out_size, "PersAutoPolicyQuoteInqRq");
    ok = ok && append_close(out, &pos, out_size, "InsuranceSvcRq");
    ok = ok && append_close(out, &pos, out_size, "ACORD");

    return ok ? pos : 0u;
}

static int text_equals(const uint8_t *buf, uint32_t cs, uint32_t ce, const char *s) {
    uint32_t n = str_len(s);
    if (ce - cs != n) {
        return 0;
    }
    for (uint32_t i = 0; i < n; i++) {
        if (buf[cs + i] != (uint8_t)s[i]) {
            return 0;
        }
    }
    return 1;
}

/* Finds <wrapper_tag><FormatCurrencyAmt><Amt>N</Amt></FormatCurrencyAmt>
 * </wrapper_tag> within [start, end) and returns N via *out_cents.
 * Refuses (returns 0) if any of the three nested elements is missing
 * or the amount is not a valid decimal number. */
static int find_amount(const uint8_t *buf, uint32_t start, uint32_t end, const char *wrapper_tag,
                       uint32_t *out_cents, uint32_t *out_after) {
    uint32_t w_cs, w_ce, w_after, f_cs, f_ce, f_after, a_cs, a_ce, a_after;
    if (!acord_find(buf, start, end, wrapper_tag, &w_cs, &w_ce, &w_after)) {
        return 0;
    }
    if (!acord_find(buf, w_cs, w_ce, "FormatCurrencyAmt", &f_cs, &f_ce, &f_after)) {
        return 0;
    }
    if (!acord_find(buf, f_cs, f_ce, "Amt", &a_cs, &a_ce, &a_after)) {
        return 0;
    }
    if (!parse_uint_span(buf, a_cs, a_ce, out_cents)) {
        return 0;
    }
    if (out_after) {
        *out_after = w_after;
    }
    return 1;
}

/* Finds <FormatCurrencyAmt><Amt>N</Amt></FormatCurrencyAmt> directly
 * within [start, end) -- for a caller that has already located its own
 * wrapper (Limit, Deductible) and only needs the amount inside it,
 * unlike find_amount() above, which locates the wrapper itself too. */
static int find_amount_direct(const uint8_t *buf, uint32_t start, uint32_t end,
                              uint32_t *out_cents) {
    uint32_t f_cs, f_ce, f_after, a_cs, a_ce, a_after;
    if (!acord_find(buf, start, end, "FormatCurrencyAmt", &f_cs, &f_ce, &f_after)) {
        return 0;
    }
    if (!acord_find(buf, f_cs, f_ce, "Amt", &a_cs, &a_ce, &a_after)) {
        return 0;
    }
    return parse_uint_span(buf, a_cs, a_ce, out_cents);
}

int acord_parse_request(const uint8_t *buf, uint32_t len, acord_request_t *out) {
    uint8_t *raw = (uint8_t *)out;
    for (uint32_t i = 0; i < sizeof(*out); i++) {
        raw[i] = 0;
    }

    uint32_t doc_cs, doc_ce, doc_after;
    if (!acord_find(buf, 0, len, "ACORD", &doc_cs, &doc_ce, &doc_after) || doc_after != len) {
        return 0; /* trailing bytes are refused, not ignored */
    }
    uint32_t svc_cs, svc_ce, svc_after;
    if (!acord_find(buf, doc_cs, doc_ce, "InsuranceSvcRq", &svc_cs, &svc_ce, &svc_after)) {
        return 0;
    }
    uint32_t iid_cs, iid_ce, iid_after;
    if (!acord_find(buf, svc_cs, svc_ce, "ItemIdInfo", &iid_cs, &iid_ce, &iid_after)) {
        return 0;
    }
    uint32_t rq_cs, rq_ce, rq_after;
    if (!acord_find(buf, svc_cs, svc_ce, "PersAutoPolicyQuoteInqRq", &rq_cs, &rq_ce, &rq_after)) {
        return 0;
    }

    uint32_t iop_cs, iop_ce, iop_after;
    if (!acord_find(buf, rq_cs, rq_ce, "InsuredOrPrincipal", &iop_cs, &iop_ce, &iop_after)) {
        return 0;
    }
    {
        uint32_t gpi_cs, gpi_ce, gpi_after;
        if (!acord_find(buf, iop_cs, iop_ce, "GeneralPartyInfo", &gpi_cs, &gpi_ce, &gpi_after)) {
            return 0;
        }
        uint32_t ni_cs, ni_ce, ni_after;
        if (!acord_find(buf, gpi_cs, gpi_ce, "NameInfo", &ni_cs, &ni_ce, &ni_after)) {
            return 0;
        }
        uint32_t pn_cs, pn_ce, pn_after;
        if (!acord_find(buf, ni_cs, ni_ce, "PersonName", &pn_cs, &pn_ce, &pn_after)) {
            return 0;
        }
        uint32_t sn_cs, sn_ce, sn_after, gn_cs, gn_ce, gn_after;
        if (!acord_find(buf, pn_cs, pn_ce, "Surname", &sn_cs, &sn_ce, &sn_after) ||
            !copy_text_span(buf, sn_cs, sn_ce, out->surname, sizeof(out->surname))) {
            return 0;
        }
        if (!acord_find(buf, pn_cs, pn_ce, "GivenName", &gn_cs, &gn_ce, &gn_after) ||
            !copy_text_span(buf, gn_cs, gn_ce, out->given_name, sizeof(out->given_name))) {
            return 0;
        }
        uint32_t addr_cs, addr_ce, addr_after;
        if (!acord_find(buf, gpi_cs, gpi_ce, "Addr", &addr_cs, &addr_ce, &addr_after)) {
            return 0;
        }
        uint32_t sp_cs, sp_ce, sp_after, pc_cs, pc_ce, pc_after;
        if (!acord_find(buf, addr_cs, addr_ce, "StateProvCd", &sp_cs, &sp_ce, &sp_after) ||
            !copy_text_span(buf, sp_cs, sp_ce, out->state_prov_cd, sizeof(out->state_prov_cd))) {
            return 0;
        }
        if (!acord_find(buf, addr_cs, addr_ce, "PostalCode", &pc_cs, &pc_ce, &pc_after) ||
            !copy_text_span(buf, pc_cs, pc_ce, out->postal_code, sizeof(out->postal_code))) {
            return 0;
        }
    }
    {
        uint32_t pd_cs, pd_ce, pd_after;
        if (!acord_find(buf, iop_cs, iop_ce, "PersDriverInfo", &pd_cs, &pd_ce, &pd_after)) {
            return 0;
        }
        uint32_t age_cs, age_ce, age_after, yl_cs, yl_ce, yl_after, af_cs, af_ce, af_after;
        if (!acord_find(buf, pd_cs, pd_ce, "Age", &age_cs, &age_ce, &age_after) ||
            !parse_uint_span(buf, age_cs, age_ce, &out->applicant.driver_age)) {
            return 0;
        }
        if (!acord_find(buf, pd_cs, pd_ce, "YearsLicensed", &yl_cs, &yl_ce, &yl_after) ||
            !parse_uint_span(buf, yl_cs, yl_ce, &out->applicant.years_licensed)) {
            return 0;
        }
        if (!acord_find(buf, pd_cs, pd_ce, "AtFaultAccidentCnt", &af_cs, &af_ce, &af_after) ||
            !parse_uint_span(buf, af_cs, af_ce, &out->applicant.at_fault_accidents_3yr)) {
            return 0;
        }
    }
    {
        uint32_t pv_cs, pv_ce, pv_after;
        if (!acord_find(buf, iop_cs, iop_ce, "PersVehInfo", &pv_cs, &pv_ce, &pv_after)) {
            return 0;
        }
        uint32_t dummy_after;
        if (!find_amount(buf, pv_cs, pv_ce, "VehCurrentValue", &out->applicant.vehicle_value_cents,
                         &dummy_after)) {
            return 0;
        }
        uint32_t va_cs, va_ce, va_after;
        if (!acord_find(buf, pv_cs, pv_ce, "VehAge", &va_cs, &va_ce, &va_after) ||
            !parse_uint_span(buf, va_cs, va_ce, &out->applicant.vehicle_age_years)) {
            return 0;
        }
    }
    {
        uint32_t tt_cs, tt_ce, tt_after;
        if (!acord_find(buf, rq_cs, rq_ce, "TerritoryTierCd", &tt_cs, &tt_ce, &tt_after) ||
            !parse_uint_span(buf, tt_cs, tt_ce, &out->applicant.territory_tier)) {
            return 0;
        }
    }

    uint32_t pol_cs, pol_ce, pol_after;
    if (!acord_find(buf, rq_cs, rq_ce, "Policy", &pol_cs, &pol_ce, &pol_after)) {
        return 0;
    }
    {
        uint32_t lob_cs, lob_ce, lob_after;
        if (!acord_find(buf, pol_cs, pol_ce, "LOBCd", &lob_cs, &lob_ce, &lob_after) ||
            !text_equals(buf, lob_cs, lob_ce, "AUTOP")) {
            return 0;
        }
        uint32_t ct_cs, ct_ce, ct_after;
        if (!acord_find(buf, pol_cs, pol_ce, "ContractTerm", &ct_cs, &ct_ce, &ct_after)) {
            return 0;
        }
        uint32_t ed_cs, ed_ce, ed_after, xd_cs, xd_ce, xd_after;
        if (!acord_find(buf, ct_cs, ct_ce, "EffectiveDt", &ed_cs, &ed_ce, &ed_after) ||
            !copy_text_span(buf, ed_cs, ed_ce, out->effective_date, sizeof(out->effective_date))) {
            return 0;
        }
        if (!acord_find(buf, ct_cs, ct_ce, "ExpirationDt", &xd_cs, &xd_ce, &xd_after) ||
            !copy_text_span(buf, xd_cs, xd_ce, out->expiration_date, sizeof(out->expiration_date))) {
            return 0;
        }
    }
    {
        uint32_t cov_cs, cov_ce, cov_after;
        if (!acord_find(buf, pol_cs, pol_ce, "Coverage", &cov_cs, &cov_ce, &cov_after)) {
            return 0;
        }
        uint32_t cd_cs, cd_ce, cd_after;
        if (!acord_find(buf, cov_cs, cov_ce, "CoverageCd", &cd_cs, &cd_ce, &cd_after) ||
            !text_equals(buf, cd_cs, cd_ce, "BI")) {
            return 0;
        }
        uint32_t lim1_cs, lim1_ce, lim1_after;
        if (!acord_find(buf, cov_cs, cov_ce, "Limit", &lim1_cs, &lim1_ce, &lim1_after)) {
            return 0;
        }
        if (!find_amount_direct(buf, lim1_cs, lim1_ce, &out->applicant.bi_per_person_cents)) {
            return 0;
        }
        uint32_t lac1_cs, lac1_ce, lac1_after;
        if (!acord_find(buf, lim1_cs, lim1_ce, "LimitAppliesToCd", &lac1_cs, &lac1_ce, &lac1_after) ||
            !text_equals(buf, lac1_cs, lac1_ce, "PerPerson")) {
            return 0;
        }
        uint32_t lim2_cs, lim2_ce, lim2_after;
        if (!acord_find(buf, lim1_after, cov_ce, "Limit", &lim2_cs, &lim2_ce, &lim2_after)) {
            return 0;
        }
        if (!find_amount_direct(buf, lim2_cs, lim2_ce, &out->applicant.bi_per_accident_cents)) {
            return 0;
        }
        uint32_t lac2_cs, lac2_ce, lac2_after;
        if (!acord_find(buf, lim2_cs, lim2_ce, "LimitAppliesToCd", &lac2_cs, &lac2_ce, &lac2_after) ||
            !text_equals(buf, lac2_cs, lac2_ce, "PerAcc")) {
            return 0;
        }
    }
    {
        uint32_t cov_cs, cov_ce, cov_after, first_cov_after;
        if (!acord_find(buf, pol_cs, pol_ce, "Coverage", &cov_cs, &cov_ce, &cov_after)) {
            return 0;
        }
        first_cov_after = cov_after;
        if (!acord_find(buf, first_cov_after, pol_ce, "Coverage", &cov_cs, &cov_ce, &cov_after)) {
            return 0;
        }
        uint32_t cd_cs, cd_ce, cd_after;
        if (!acord_find(buf, cov_cs, cov_ce, "CoverageCd", &cd_cs, &cd_ce, &cd_after) ||
            !text_equals(buf, cd_cs, cd_ce, "PD")) {
            return 0;
        }
        uint32_t lim_cs, lim_ce, lim_after;
        if (!acord_find(buf, cov_cs, cov_ce, "Limit", &lim_cs, &lim_ce, &lim_after)) {
            return 0;
        }
        if (!find_amount_direct(buf, lim_cs, lim_ce, &out->applicant.pd_cents)) {
            return 0;
        }
        uint32_t third_cov_cs, third_cov_ce, third_cov_after;
        if (!acord_find(buf, cov_after, pol_ce, "Coverage", &third_cov_cs, &third_cov_ce,
                       &third_cov_after)) {
            return 0;
        }
        uint32_t cd3_cs, cd3_ce, cd3_after;
        if (!acord_find(buf, third_cov_cs, third_cov_ce, "CoverageCd", &cd3_cs, &cd3_ce, &cd3_after) ||
            !text_equals(buf, cd3_cs, cd3_ce, "COLL")) {
            return 0;
        }
        uint32_t ded_cs, ded_ce, ded_after;
        if (!acord_find(buf, third_cov_cs, third_cov_ce, "Deductible", &ded_cs, &ded_ce, &ded_after)) {
            return 0;
        }
        if (!find_amount_direct(buf, ded_cs, ded_ce, &out->applicant.collision_deductible_cents)) {
            return 0;
        }
    }
    return 1;
}

/* ---------------------------------------------------------------- */
/* Response                                                          */
/* ---------------------------------------------------------------- */

uint32_t acord_build_response(const acord_response_t *r, uint8_t *out, uint32_t out_size) {
    if (r->quote_count == 0u || r->quote_count > INS_MAX_CARRIERS) {
        return 0;
    }
    uint32_t pos = 0;
    int ok = 1;
    ok = ok && append_open(out, &pos, out_size, "ACORD");
    ok = ok && append_open(out, &pos, out_size, "InsuranceSvcRs");
    ok = ok && append_open(out, &pos, out_size, "ItemIdInfo");
    ok = ok && append_open(out, &pos, out_size, "OtherIdentifier");
    ok = ok && append_leaf(out, &pos, out_size, "OtherIdTypeCd", "Response");
    ok = ok && append_leaf(out, &pos, out_size, "UID", "033-CH34-DEMO-0001");
    ok = ok && append_close(out, &pos, out_size, "OtherIdentifier");
    ok = ok && append_close(out, &pos, out_size, "ItemIdInfo");
    ok = ok && append_open(out, &pos, out_size, "PersAutoPolicyQuoteInqRs");

    for (uint32_t i = 0; i < r->quote_count; i++) {
        const ins_quote_t *q = &r->quotes[i];
        ok = ok && append_open(out, &pos, out_size, "Policy");
        ok = ok && append_open(out, &pos, out_size, "GeneralPartyInfo");
        ok = ok && append_open(out, &pos, out_size, "NameInfo");
        ok = ok && append_open(out, &pos, out_size, "CommlName");
        ok = ok && append_leaf(out, &pos, out_size, "CommercialName", q->carrier_name);
        ok = ok && append_close(out, &pos, out_size, "CommlName");
        ok = ok && append_close(out, &pos, out_size, "NameInfo");
        ok = ok && append_close(out, &pos, out_size, "GeneralPartyInfo");
        ok = ok && append_leaf(out, &pos, out_size, "PolicyStatusCd", "Quoted");
        ok = ok && append_open(out, &pos, out_size, "PolicyAmt");
        ok = ok && append_amount(out, &pos, out_size, q->premium_cents);
        ok = ok && append_close(out, &pos, out_size, "PolicyAmt");
        ok = ok && append_close(out, &pos, out_size, "Policy");
    }

    ok = ok && append_close(out, &pos, out_size, "PersAutoPolicyQuoteInqRs");
    ok = ok && append_close(out, &pos, out_size, "InsuranceSvcRs");
    ok = ok && append_close(out, &pos, out_size, "ACORD");
    return ok ? pos : 0u;
}

int acord_parse_response(const uint8_t *buf, uint32_t len, acord_response_t *out) {
    uint8_t *raw = (uint8_t *)out;
    for (uint32_t i = 0; i < sizeof(*out); i++) {
        raw[i] = 0;
    }
    uint32_t doc_cs, doc_ce, doc_after;
    if (!acord_find(buf, 0, len, "ACORD", &doc_cs, &doc_ce, &doc_after) || doc_after != len) {
        return 0;
    }
    uint32_t svc_cs, svc_ce, svc_after;
    if (!acord_find(buf, doc_cs, doc_ce, "InsuranceSvcRs", &svc_cs, &svc_ce, &svc_after)) {
        return 0;
    }
    uint32_t rs_cs, rs_ce, rs_after;
    if (!acord_find(buf, svc_cs, svc_ce, "PersAutoPolicyQuoteInqRs", &rs_cs, &rs_ce, &rs_after)) {
        return 0;
    }

    uint32_t cursor = rs_cs;
    uint32_t count = 0;
    while (count < INS_MAX_CARRIERS) {
        uint32_t pol_cs, pol_ce, pol_after;
        if (!acord_find(buf, cursor, rs_ce, "Policy", &pol_cs, &pol_ce, &pol_after)) {
            break;
        }
        uint32_t gpi_cs, gpi_ce, gpi_after;
        if (!acord_find(buf, pol_cs, pol_ce, "GeneralPartyInfo", &gpi_cs, &gpi_ce, &gpi_after)) {
            return 0;
        }
        uint32_t ni_cs, ni_ce, ni_after;
        if (!acord_find(buf, gpi_cs, gpi_ce, "NameInfo", &ni_cs, &ni_ce, &ni_after)) {
            return 0;
        }
        uint32_t cn_cs, cn_ce, cn_after;
        if (!acord_find(buf, ni_cs, ni_ce, "CommlName", &cn_cs, &cn_ce, &cn_after)) {
            return 0;
        }
        uint32_t name_cs, name_ce, name_after;
        if (!acord_find(buf, cn_cs, cn_ce, "CommercialName", &name_cs, &name_ce, &name_after) ||
            !copy_text_span(buf, name_cs, name_ce, out->quotes[count].carrier_name,
                           sizeof(out->quotes[count].carrier_name))) {
            return 0;
        }
        uint32_t st_cs, st_ce, st_after;
        if (!acord_find(buf, pol_cs, pol_ce, "PolicyStatusCd", &st_cs, &st_ce, &st_after) ||
            !text_equals(buf, st_cs, st_ce, "Quoted")) {
            return 0;
        }
        uint32_t amt_after;
        if (!find_amount(buf, pol_cs, pol_ce, "PolicyAmt", &out->quotes[count].premium_cents,
                         &amt_after)) {
            return 0;
        }
        count++;
        cursor = pol_after;
    }
    if (count == 0u) {
        return 0;
    }
    out->quote_count = count;
    return 1;
}
