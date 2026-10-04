/* Chapter 49: the 837P claim reader (see 049_claim.h). */
#include "049_claim.h"

static void zero_claim(cl_claim_t *c) { char *p = (char *)c; for (uint32_t i = 0; i < sizeof *c; i++) { p[i] = 0; } }
static int copy_n(char *dst, uint32_t cap, const char *s, uint16_t n) { if (n >= cap) { return -1; } for (uint16_t i = 0; i < n; i++) { dst[i] = s[i]; } dst[n] = 0; return 0; }

const char *cl_strerror(int rc) {
    switch (rc) {
    case CL_OK: return "ok"; case CL_ERR_X12: return "the X12 envelope is invalid"; case CL_ERR_NOT_837: return "a transaction set is not an 837"; case CL_ERR_VERSION: return "not version 005010X222A1 (837 Professional)";
    case CL_ERR_BHT: return "missing or invalid BHT segment"; case CL_ERR_HL: return "HL hierarchy invalid"; case CL_ERR_UNSUPPORTED: return "a claim shape this reader does not support";
    case CL_ERR_NPI: return "invalid NPI"; case CL_ERR_ELEMENT: return "a required element is missing or invalid"; case CL_ERR_STRUCTURE: return "segments out of order"; case CL_ERR_BALANCE: return "claim total does not equal the sum of its service lines";
    case CL_ERR_LIMIT: return "more claims or service lines than the tables hold";
    }
    return "unknown error";
}

int cl_npi_valid(const char *npi) {
    for (int i = 0; i < 10; i++) { if (npi[i] < '0' || npi[i] > '9') { return 0; } }
    /* Luhn over the 15 digits "80840" + the ten NPI digits: from the right, double every second digit, subtract 9 from any result above 9; the sum must end in 0 */
    char all[15] = {'8', '0', '8', '4', '0'}; for (int i = 0; i < 10; i++) { all[5 + i] = npi[i]; }
    int sum = 0;
    for (int i = 14, pos = 0; i >= 0; i--, pos++) { int d = all[i] - '0'; if (pos % 2 == 1) { d *= 2; if (d > 9) { d -= 9; } } sum += d; }
    return sum % 10 == 0;
}

#define FAIL(code, text) do { b->err_seg = (uint16_t)si; b->msg = (text); return (code); } while (0)
typedef struct { int hl_n, hl20, hl22; int have_bht; int have_bill, have_sub, have_payer; int in_claim; int cur; int line_open; int dos_claim_ok; int32_t claim_dos; uint8_t last_dx_n; } st_t;

static int finish_claim(cl_batch_t *b, st_t *s, int si) {
    if (!s->in_claim) { return CL_OK; }
    cl_claim_t *c = &b->claim[s->cur];
    if (c->n_line == 0) { FAIL(CL_ERR_STRUCTURE, "a claim (CLM) has no service line (SV1)"); }
    int64_t sum = 0; for (int i = 0; i < c->n_line; i++) { sum += c->line[i].charge; }
    if (sum != c->total) { FAIL(CL_ERR_BALANCE, "CLM02 total charge is not the sum of the SV102 line charges"); }
    s->in_claim = 0; return CL_OK;
}

int cl_parse(cl_batch_t *b, const x12_t *x) {
    b->n_claim = 0; b->err_seg = 0; b->msg = "";
    int si = 0;
    if (x->n_seg == 0) { FAIL(CL_ERR_X12, "empty"); }
    uint16_t el; const char *e;
    /* the group's version: GS08 */
    for (uint16_t i = 0; i < x->n_seg; i++) {
        si = i;
        if (!x12_is(&x->seg[i], "ST")) { continue; }
        e = x12_el(&x->seg[i], 1, &el); if (!e || !x12_str_eq(e, el, "837")) { FAIL(CL_ERR_NOT_837, "ST01 is not 837"); }
        e = x12_el(&x->seg[i], 3, &el); if (!e || !x12_str_eq(e, el, "005010X222A1")) { FAIL(CL_ERR_VERSION, "ST03 is not 005010X222A1"); }
        st_t s; s.hl_n = 0; s.hl20 = s.hl22 = 0; s.have_bht = 0; s.have_bill = s.have_sub = s.have_payer = 0; s.in_claim = 0; s.cur = -1; s.line_open = 0; s.dos_claim_ok = 0; s.claim_dos = 0; s.last_dx_n = 0;
        char bill_npi[11] = {0}, bill_name[40] = {0}, sub_id[26] = {0}, sub_last[36] = {0}, sub_first[26] = {0}, pay_id[26] = {0}, pay_name[40] = {0};
        uint16_t j = (uint16_t)(i + 1);
        for (; j < x->n_seg && !x12_is(&x->seg[j], "SE"); j++) {
            si = j; const x12_seg_t *g = &x->seg[j]; uint16_t l1, l2, l3; const char *e1, *e2, *e3;
            if (!s.have_bht) {
                if (!x12_is(g, "BHT")) { FAIL(CL_ERR_BHT, "the first segment after ST must be BHT"); }
                e1 = x12_el(g, 1, &l1); e2 = x12_el(g, 2, &l2); e3 = x12_el(g, 6, &l3);
                if (!e1 || !x12_str_eq(e1, l1, "0019") || !e2 || !(x12_str_eq(e2, l2, "00") || x12_str_eq(e2, l2, "18")) || !e3 || !(x12_str_eq(e3, l3, "CH") || x12_str_eq(e3, l3, "RP"))) { FAIL(CL_ERR_BHT, "BHT01 must be 0019, BHT02 00 or 18, BHT06 CH or RP"); }
                s.have_bht = 1; continue;
            }
            if (x12_is(g, "HL")) {
                int r = finish_claim(b, &s, j); if (r) { return r; }
                uint32_t id, parent = 0; const char *p1 = x12_el(g, 1, &l1), *p2 = x12_el(g, 2, &l2), *p3 = x12_el(g, 3, &l3);
                if (!p1 || x12_uint(p1, l1, &id) != 0 || id != (uint32_t)(s.hl_n + 1)) { FAIL(CL_ERR_HL, "HL01 must count 1, 2, 3, ... in order"); }
                if (!p3) { FAIL(CL_ERR_HL, "HL03 (level code) missing"); }
                if (x12_str_eq(p3, l3, "20")) {
                    if (p2 && l2 != 0) { FAIL(CL_ERR_HL, "a billing-provider HL has no parent"); }
                    s.hl20 = (int)id; s.hl22 = 0; s.have_bill = 0; s.have_sub = 0; s.have_payer = 0;
                } else if (x12_str_eq(p3, l3, "22")) {
                    if (!p2 || x12_uint(p2, l2, &parent) != 0 || (int)parent != s.hl20 || s.hl20 == 0) { FAIL(CL_ERR_HL, "a subscriber HL must name the billing-provider HL as its parent"); }
                    s.hl22 = (int)id; s.have_sub = 0; s.have_payer = 0;
                } else if (x12_str_eq(p3, l3, "23")) { FAIL(CL_ERR_UNSUPPORTED, "claims for a dependent patient (HL level 23) are not supported: the patient must be the subscriber"); }
                else { FAIL(CL_ERR_HL, "HL03 must be 20 or 22"); }
                s.hl_n = (int)id; continue;
            }
            if (x12_is(g, "NM1")) {
                e1 = x12_el(g, 1, &l1); if (!e1) { FAIL(CL_ERR_ELEMENT, "NM101 missing"); }
                if (x12_str_eq(e1, l1, "85")) {
                    if (s.hl20 == 0 || s.hl22 != 0) { FAIL(CL_ERR_STRUCTURE, "NM1*85 (billing provider) is only valid in the billing-provider loop"); }
                    e2 = x12_el(g, 8, &l2); e3 = x12_el(g, 9, &l3);
                    if (!e2 || !x12_str_eq(e2, l2, "XX") || !e3 || l3 != 10) { FAIL(CL_ERR_NPI, "the billing provider needs NM108 = XX and a 10-digit NPI in NM109"); }
                    if (copy_n(bill_npi, 11, e3, l3) != 0 || !cl_npi_valid(bill_npi)) { FAIL(CL_ERR_NPI, "the billing provider's NPI fails the check-digit test"); }
                    const char *nm = x12_el(g, 3, &l2); if (!nm || copy_n(bill_name, 40, nm, l2) != 0) { FAIL(CL_ERR_ELEMENT, "billing provider name missing or too long"); }
                    s.have_bill = 1;
                } else if (x12_str_eq(e1, l1, "IL")) {
                    if (s.hl22 == 0) { FAIL(CL_ERR_STRUCTURE, "NM1*IL (subscriber) outside a subscriber loop"); }
                    e2 = x12_el(g, 8, &l2); e3 = x12_el(g, 9, &l3);
                    if (!e2 || !x12_str_eq(e2, l2, "MI") || !e3 || l3 == 0 || copy_n(sub_id, 26, e3, l3) != 0) { FAIL(CL_ERR_ELEMENT, "the subscriber needs NM108 = MI and a member id (at most 25 characters) in NM109"); }
                    const char *ln = x12_el(g, 3, &l2), *fn = x12_el(g, 4, &l3);
                    if (!ln || copy_n(sub_last, 36, ln, l2) != 0 || !fn || copy_n(sub_first, 26, fn, l3) != 0) { FAIL(CL_ERR_ELEMENT, "subscriber name missing or too long"); }
                    s.have_sub = 1;
                } else if (x12_str_eq(e1, l1, "PR")) {
                    if (s.hl22 == 0) { FAIL(CL_ERR_STRUCTURE, "NM1*PR (payer) outside a subscriber loop"); }
                    e2 = x12_el(g, 8, &l2); e3 = x12_el(g, 9, &l3);
                    if (!e2 || !(x12_str_eq(e2, l2, "PI") || x12_str_eq(e2, l2, "XV")) || !e3 || l3 == 0 || copy_n(pay_id, 26, e3, l3) != 0) { FAIL(CL_ERR_ELEMENT, "the payer needs NM108 = PI or XV and an id in NM109"); }
                    const char *nm = x12_el(g, 3, &l2); if (!nm || copy_n(pay_name, 40, nm, l2) != 0) { FAIL(CL_ERR_ELEMENT, "payer name missing or too long"); }
                    s.have_payer = 1;
                }
                continue;
            }
            if (x12_is(g, "SBR")) {
                e1 = x12_el(g, 1, &l1); e2 = x12_el(g, 2, &l2);
                if (!e1 || l1 != 1 || !(e1[0] == 'P' || e1[0] == 'S' || e1[0] == 'T')) { FAIL(CL_ERR_ELEMENT, "SBR01 (payer responsibility) must be P, S or T"); }
                if (e2 && l2 != 0 && !x12_str_eq(e2, l2, "18")) { FAIL(CL_ERR_UNSUPPORTED, "SBR02 must be empty or 18 (the subscriber is the patient)"); }
                continue;
            }
            if (x12_is(g, "CLM")) {
                int r = finish_claim(b, &s, j); if (r) { return r; }
                if (!s.have_bill || !s.have_sub || !s.have_payer) { FAIL(CL_ERR_STRUCTURE, "CLM before the billing provider, subscriber and payer were given"); }
                if (b->n_claim >= CL_MAX_CLAIM) { FAIL(CL_ERR_LIMIT, "more than 8 claims"); }
                cl_claim_t *c = &b->claim[b->n_claim]; zero_claim(c); s.cur = b->n_claim; b->n_claim++; s.in_claim = 1; s.line_open = 0; s.dos_claim_ok = 0;
                e1 = x12_el(g, 1, &l1); e2 = x12_el(g, 2, &l2); e3 = x12_el(g, 5, &l3);
                if (!e1 || l1 == 0 || l1 > 38 || copy_n(c->pcn, 40, e1, l1) != 0) { FAIL(CL_ERR_ELEMENT, "CLM01 (patient control number) missing or longer than 38 characters"); }
                if (!e2 || x12_money(e2, l2, &c->total) != 0 || c->total <= 0) { FAIL(CL_ERR_ELEMENT, "CLM02 (total charge) must be a positive amount with at most two decimals"); }
                if (!e3) { FAIL(CL_ERR_ELEMENT, "CLM05 missing"); }
                { uint16_t pl; const char *pc = x12_comp(x, e3, l3, 0, &pl); if (!pc || pl != 2 || pc[0] < '0' || pc[0] > '9' || pc[1] < '0' || pc[1] > '9') { FAIL(CL_ERR_ELEMENT, "CLM05-1 (place of service) must be two digits"); } c->pos[0] = pc[0]; c->pos[1] = pc[1]; c->pos[2] = 0; }
                for (int k = 0; k < 11; k++) { c->bill_npi[k] = bill_npi[k]; } for (int k = 0; k < 40; k++) { c->bill_name[k] = bill_name[k]; c->payer_name[k] = pay_name[k]; }
                for (int k = 0; k < 26; k++) { c->sub_id[k] = sub_id[k]; c->sub_first[k] = sub_first[k]; c->payer_id[k] = pay_id[k]; } for (int k = 0; k < 36; k++) { c->sub_last[k] = sub_last[k]; }
                continue;
            }
            if (!s.in_claim) { continue; }
            cl_claim_t *c = &b->claim[s.cur];
            if (x12_is(g, "HI")) {
                for (int k = 1; k <= g->n_el; k++) {
                    e1 = x12_el(g, k, &l1); if (!e1) { break; }
                    uint16_t ql, cl; const char *q = x12_comp(x, e1, l1, 0, &ql), *cd = x12_comp(x, e1, l1, 1, &cl);
                    if (!q || !cd) { FAIL(CL_ERR_ELEMENT, "HI elements must be composites such as ABK:J0300"); }
                    int principal = x12_str_eq(q, ql, "ABK") || x12_str_eq(q, ql, "BK"); int other = x12_str_eq(q, ql, "ABF") || x12_str_eq(q, ql, "BF");
                    if (!principal && !other) { continue; }
                    if (k == 1 && !principal) { FAIL(CL_ERR_ELEMENT, "HI01 must carry the principal diagnosis (ABK or BK)"); }
                    if (cl < 3 || cl > 7) { FAIL(CL_ERR_ELEMENT, "a diagnosis code has 3 to 7 characters"); }
                    for (uint16_t m = 0; m < cl; m++) { char ch = cd[m]; if (!((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z'))) { FAIL(CL_ERR_ELEMENT, "a diagnosis code is upper-case letters and digits, without a decimal point"); } }
                    if (c->n_dx >= CL_MAX_DX) { FAIL(CL_ERR_LIMIT, "more than 12 diagnoses"); }
                    copy_n(c->dx[c->n_dx], 9, cd, cl); c->n_dx++;
                }
                continue;
            }
            if (x12_is(g, "LX")) {
                e1 = x12_el(g, 1, &l1); uint32_t ln;
                if (!e1 || x12_uint(e1, l1, &ln) != 0 || ln != (uint32_t)(c->n_line + 1)) { FAIL(CL_ERR_STRUCTURE, "LX01 must count 1, 2, 3, ... within a claim"); }
                if (c->n_line >= CL_MAX_LINE) { FAIL(CL_ERR_LIMIT, "more than 16 service lines in a claim"); }
                s.line_open = 1; cl_line_t *ln_ = &c->line[c->n_line]; for (uint32_t k = 0; k < sizeof *ln_; k++) { ((char *)ln_)[k] = 0; } c->n_line++; ln_->dos = -1; continue;
            }
            if (x12_is(g, "SV1")) {
                if (!s.line_open || c->line[c->n_line - 1].charge != 0) { FAIL(CL_ERR_STRUCTURE, "SV1 must follow its LX, once"); }
                cl_line_t *ln_ = &c->line[c->n_line - 1];
                e1 = x12_el(g, 1, &l1); e2 = x12_el(g, 2, &l2); e3 = x12_el(g, 4, &l3);
                if (!e1) { FAIL(CL_ERR_ELEMENT, "SV101 missing"); }
                { uint16_t ql, pl, ml; const char *q = x12_comp(x, e1, l1, 0, &ql), *pc = x12_comp(x, e1, l1, 1, &pl);
                  if (!q || !x12_str_eq(q, ql, "HC") || !pc || pl != 5) { FAIL(CL_ERR_ELEMENT, "SV101 must be HC:<5-character procedure code>"); }
                  for (int m = 0; m < 5; m++) { char ch = pc[m]; if (!((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z'))) { FAIL(CL_ERR_ELEMENT, "a procedure code is upper-case letters and digits"); } }
                  copy_n(ln_->code, 6, pc, pl);
                  const char *md = x12_comp(x, e1, l1, 2, &ml); if (md) { if (ml != 2 || copy_n(ln_->mod, 3, md, ml) != 0) { FAIL(CL_ERR_ELEMENT, "a procedure modifier has two characters"); } } }
                if (!e2 || x12_money(e2, l2, &ln_->charge) != 0 || ln_->charge <= 0) { FAIL(CL_ERR_ELEMENT, "SV102 (line charge) must be a positive amount with at most two decimals"); }
                { const char *u = x12_el(g, 3, &l2); if (!u || !x12_str_eq(u, l2, "UN")) { FAIL(CL_ERR_ELEMENT, "SV103 must be UN (units)"); } }
                if (!e3 || x12_uint(e3, l3, &ln_->units) != 0 || ln_->units < 1 || ln_->units > 999) { FAIL(CL_ERR_ELEMENT, "SV104 (units) must be a whole number from 1 to 999"); }
                { const char *dp = x12_el(g, 7, &l2); if (dp && l2) { uint16_t pl; const char *first = x12_comp(x, dp, l2, 0, &pl); uint32_t ptr; if (!first || x12_uint(first, pl, &ptr) != 0 || ptr < 1 || ptr > c->n_dx) { FAIL(CL_ERR_ELEMENT, "SV107 points at a diagnosis that does not exist"); } ln_->dx_ptr = (uint8_t)ptr; } }
                continue;
            }
            if (x12_is(g, "DTP")) {
                e1 = x12_el(g, 1, &l1); e2 = x12_el(g, 2, &l2); e3 = x12_el(g, 3, &l3);
                if (!e1 || !x12_str_eq(e1, l1, "472")) { continue; }
                int32_t d; const char *ds = e3; uint16_t dl = l3;
                if (!e2 || !e3) { FAIL(CL_ERR_ELEMENT, "DTP*472 needs a format and a date"); }
                if (x12_str_eq(e2, l2, "RD8")) { if (l3 != 17 || e3[8] != '-') { FAIL(CL_ERR_ELEMENT, "DTP RD8 range must be CCYYMMDD-CCYYMMDD"); } dl = 8; } /* the service starts on the first date */
                else if (!x12_str_eq(e2, l2, "D8")) { FAIL(CL_ERR_ELEMENT, "DTP*472 format must be D8 or RD8"); }
                if (x12_date(ds, dl, &d) != 0) { FAIL(CL_ERR_ELEMENT, "DTP*472 is not a valid calendar date"); }
                if (s.line_open) { c->line[c->n_line - 1].dos = d; } else { s.claim_dos = d; s.dos_claim_ok = 1; }
                continue;
            }
        }
        si = j;
        if (j >= x->n_seg) { FAIL(CL_ERR_X12, "no SE"); }
        { int r = finish_claim(b, &s, j); if (r) { return r; } }
        if (!s.have_bht) { si = i; FAIL(CL_ERR_BHT, "no BHT in the transaction"); }
        /* every line needs a date: its own, else the claim-level DTP*472 */
        for (int k = 0; k < b->n_claim; k++) { for (int m = 0; m < b->claim[k].n_line; m++) { if (b->claim[k].line[m].dos < 0) { if (s.dos_claim_ok) { b->claim[k].line[m].dos = s.claim_dos; } else { si = j; FAIL(CL_ERR_ELEMENT, "a service line has no date of service (DTP*472)"); } } } }
        i = j;
    }
    if (b->n_claim == 0) { si = 0; FAIL(CL_ERR_STRUCTURE, "the file contains no claim"); }
    return CL_OK;
}
