/* Chapter 49: the 835 builder and reconciling reader (see 055_remit.h). */
#include "055_remit.h"

static uint32_t cstr_len(const char *s) { uint32_t n = 0; while (s[n]) { n++; } return n; }
typedef struct { char *b; uint32_t cap, o; int bad; } out_t;
static void pc(out_t *w, char c) { if (w->o + 1 >= w->cap) { w->bad = 1; return; } w->b[w->o++] = c; }
static void ps(out_t *w, const char *s) { while (*s) { pc(w, *s++); } }
static void pn(out_t *w, uint32_t v, int pad) { char t[12]; int n = 0; while (v || n < pad) { t[n++] = (char)('0' + (int)(v % 10u)); v /= 10u; if (n >= 11) { break; } } while (n) { pc(w, t[--n]); } }
static void pm(out_t *w, int64_t cents) { char t[28]; x12_money_str(cents, t); ps(w, t); }
static void padded(out_t *w, const char *s, int width) { int n = (int)cstr_len(s); for (int i = 0; i < width; i++) { pc(w, i < n ? s[i] : ' '); } }
void rm_date_str(int32_t days, char *o) { /* inverse of the days-since-epoch count */
    int32_t z = days + 719468, era = (z >= 0 ? z : z - 146096) / 146097, doe = z - era * 146097; int32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365, y = yoe + era * 400, doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;
    int32_t d = doy - (153 * mp + 2) / 5 + 1, m = mp + (mp < 10 ? 3 : -9); y += (m <= 2);
    if (y < 1 || y > 9999) { for (int i = 0; i < 8; i++) { o[i] = '0'; } o[8] = 0; return; }
    o[0] = (char)('0' + y / 1000); o[1] = (char)('0' + y / 100 % 10); o[2] = (char)('0' + y / 10 % 10); o[3] = (char)('0' + y % 10); o[4] = (char)('0' + m / 10); o[5] = (char)('0' + m % 10); o[6] = (char)('0' + d / 10); o[7] = (char)('0' + d % 10); o[8] = 0;
}
static void seg(out_t *w, uint16_t *count) { ps(w, "~"); (*count)++; }
int rm_build(char *buf, uint32_t cap, const rm_hdr_t *h, const cl_claim_t *claims, const adj_claim_t *res, int n) {
    out_t w; w.b = buf; w.cap = cap; w.o = 0; w.bad = 0; char date[9]; rm_date_str(h->date, date); uint16_t cnt = 0; int64_t total = 0;
    for (int i = 0; i < n; i++) { total += res[i].total_paid; }
    ps(&w, "ISA*00*          *00*          *ZZ*"); padded(&w, h->payer_id, 15); ps(&w, "*ZZ*"); padded(&w, h->payee_npi, 15); ps(&w, "*"); ps(&w, date + 2); ps(&w, "*1200*^*00501*"); pn(&w, h->ctl, 9); ps(&w, "*0*T*:~");
    ps(&w, "GS*HP*"); ps(&w, h->payer_id); ps(&w, "*"); ps(&w, h->payee_npi); ps(&w, "*"); ps(&w, date); ps(&w, "*1200*"); pn(&w, h->ctl, 1); ps(&w, "*X*005010X221A1~");
    ps(&w, "ST*835*0001"); seg(&w, &cnt);
    ps(&w, "BPR*I*"); pm(&w, total); ps(&w, "*C*CHK************"); ps(&w, date); seg(&w, &cnt);
    ps(&w, "TRN*1*"); pn(&w, h->ctl, 1); ps(&w, "*"); ps(&w, h->payer_id); seg(&w, &cnt);
    ps(&w, "DTM*405*"); ps(&w, date); seg(&w, &cnt);
    ps(&w, "N1*PR*"); ps(&w, h->payer_name); seg(&w, &cnt);
    ps(&w, "N1*PE*"); ps(&w, h->payee_name); ps(&w, "*XX*"); ps(&w, h->payee_npi); seg(&w, &cnt);
    for (int i = 0; i < n; i++) {
        const cl_claim_t *c = &claims[i]; const adj_claim_t *r = &res[i];
        ps(&w, "LX*"); pn(&w, (uint32_t)(i + 1), 1); seg(&w, &cnt);
        ps(&w, "CLP*"); ps(&w, c->pcn); ps(&w, "*"); pn(&w, r->status, 1); ps(&w, "*"); pm(&w, r->total_charge); ps(&w, "*"); pm(&w, r->total_paid); ps(&w, "*"); pm(&w, r->total_pr); ps(&w, "*MC*ICN"); pn(&w, h->ctl, 1); ps(&w, "-"); pn(&w, (uint32_t)(i + 1), 1); seg(&w, &cnt);
        ps(&w, "NM1*QC*1*"); ps(&w, c->sub_last); ps(&w, "*"); ps(&w, c->sub_first); ps(&w, "****MI*"); ps(&w, c->sub_id); seg(&w, &cnt);
        for (int k = 0; k < r->n_line; k++) {
            const adj_line_t *l = &r->line[k]; const cl_line_t *ln = &c->line[k]; char d8[9];
            ps(&w, "SVC*HC:"); ps(&w, ln->code); ps(&w, "*"); pm(&w, l->charge); ps(&w, "*"); pm(&w, l->paid); ps(&w, "**"); pn(&w, ln->units, 1); seg(&w, &cnt);
            rm_date_str(ln->dos, d8); ps(&w, "DTM*472*"); ps(&w, d8); seg(&w, &cnt);
            for (int g = 0; g < 2; g++) {
                int any = 0; for (int a = 0; a < l->n_adj; a++) { if (l->adj[a].group == g) { any = 1; } }
                if (!any) { continue; }
                ps(&w, g == ADJ_CO ? "CAS*CO" : "CAS*PR");
                int first = 1; /* each adjustment is a triplet: reason, amount, quantity -- the quantity is left empty, so a second triplet follows "**" */
                for (int a = 0; a < l->n_adj; a++) { if (l->adj[a].group == g) { ps(&w, first ? "*" : "**"); first = 0; pn(&w, l->adj[a].carc, 1); ps(&w, "*"); pm(&w, l->adj[a].amount); } }
                seg(&w, &cnt);
            }
        }
    }
    cnt++; ps(&w, "SE*"); pn(&w, cnt, 1); ps(&w, "*0001~"); ps(&w, "GE*1*"); pn(&w, h->ctl, 1); ps(&w, "~IEA*1*"); pn(&w, h->ctl, 9); ps(&w, "~");
    if (w.bad) { return -1; } w.b[w.o] = 0; return (int)w.o;
}

#define RFAIL(code, text) do { r->err_seg = (uint16_t)si; r->msg = (text); return (code); } while (0)
static int cas_sum(const x12_seg_t *g, int64_t *sum, int64_t *pr) { /* returns -1 on a bad amount */
    uint16_t l; const char *grp = x12_el(g, 1, &l); if (!grp) { return -1; } int is_pr = x12_str_eq(grp, l, "PR");
    for (int k = 3; k <= g->n_el; k += 3) { const char *a = x12_el(g, k, &l); int64_t v; if (!a || x12_money(a, l, &v) != 0) { return -1; } *sum += v; if (is_pr) { *pr += v; } }
    return 0;
}
int rm_parse(rm_report_t *r, const x12_t *x) {
    r->n_claim = 0; r->has_plb = 0; r->plb_sum = 0; r->paid_sum = 0; r->bpr_total = 0; r->err_seg = 0; r->msg = ""; r->all_balanced = 1; int si = 0; uint16_t l1, l2; const char *e1, *e2;
    int st = -1; for (uint16_t i = 0; i < x->n_seg; i++) { if (x12_is(&x->seg[i], "ST")) { st = i; break; } }
    if (st < 0) { RFAIL(RM_ERR_NOT_835, "no ST segment"); }
    si = st; e1 = x12_el(&x->seg[st], 1, &l1); if (!e1 || !x12_str_eq(e1, l1, "835")) { RFAIL(RM_ERR_NOT_835, "ST01 is not 835"); }
    int have_bpr = 0; rm_claim_t *c = 0; rm_line_t *ln = 0;
    for (uint16_t j = (uint16_t)(st + 1); j < x->n_seg && !x12_is(&x->seg[j], "SE"); j++) {
        si = j; const x12_seg_t *g = &x->seg[j];
        if (x12_is(g, "BPR")) { e1 = x12_el(g, 2, &l1); if (!e1 || x12_money(e1, l1, &r->bpr_total) != 0) { RFAIL(RM_ERR_ELEMENT, "BPR02 is not an amount"); } have_bpr = 1; continue; }
        if (x12_is(g, "PLB")) { r->has_plb = 1; for (int k = 4; k <= g->n_el; k += 2) { e1 = x12_el(g, k, &l1); int64_t v; if (!e1 || x12_money(e1, l1, &v) != 0) { RFAIL(RM_ERR_ELEMENT, "a PLB amount is not an amount"); } r->plb_sum += v; } continue; }
        if (x12_is(g, "CLP")) {
            if (r->n_claim >= RM_MAX_CLAIM) { RFAIL(RM_ERR_LIMIT, "more than 16 claims"); }
            c = &r->claim[r->n_claim++]; ln = 0; c->n_line = 0; c->has_claim_cas = 0; c->claim_cas = c->claim_pr = 0; c->balanced = 0; c->pr_matches = 0;
            e1 = x12_el(g, 1, &l1); if (!e1 || l1 == 0 || l1 >= 40) { RFAIL(RM_ERR_ELEMENT, "CLP01 missing or too long"); } for (uint16_t k = 0; k < l1; k++) { c->pcn[k] = e1[k]; } c->pcn[l1] = 0;
            uint32_t stt; e1 = x12_el(g, 2, &l1); if (!e1 || x12_uint(e1, l1, &stt) != 0) { RFAIL(RM_ERR_ELEMENT, "CLP02 (status) is not a number"); } c->status = (int)stt;
            e1 = x12_el(g, 3, &l1); e2 = x12_el(g, 4, &l2); if (!e1 || x12_money(e1, l1, &c->charge) != 0 || !e2 || x12_money(e2, l2, &c->paid) != 0) { RFAIL(RM_ERR_ELEMENT, "CLP03/CLP04 are not amounts"); }
            e1 = x12_el(g, 5, &l1); c->patient = 0; if (e1 && l1 && x12_money(e1, l1, &c->patient) != 0) { RFAIL(RM_ERR_ELEMENT, "CLP05 is not an amount"); }
            r->paid_sum += c->paid; continue;
        }
        if (!c) { if (x12_is(g, "SVC") || x12_is(g, "CAS")) { RFAIL(RM_ERR_STRUCTURE, "an SVC or CAS segment before any CLP (claim)"); } continue; }
        if (x12_is(g, "SVC")) {
            if (c->n_line >= RM_MAX_LINE) { RFAIL(RM_ERR_LIMIT, "more than 16 service lines in a claim"); }
            ln = &c->line[c->n_line++]; ln->cas_sum = ln->pr_sum = 0; ln->n_cas = 0; ln->balanced = 0;
            e1 = x12_el(g, 2, &l1); e2 = x12_el(g, 3, &l2); if (!e1 || x12_money(e1, l1, &ln->charge) != 0 || !e2 || x12_money(e2, l2, &ln->paid) != 0) { RFAIL(RM_ERR_ELEMENT, "SVC02/SVC03 are not amounts"); }
            continue;
        }
        if (x12_is(g, "CAS")) {
            int64_t sum = 0, pr = 0; if (cas_sum(g, &sum, &pr) != 0) { RFAIL(RM_ERR_ELEMENT, "a CAS amount is not an amount"); }
            if (ln) { ln->cas_sum += sum; ln->pr_sum += pr; ln->n_cas++; } else { c->has_claim_cas = 1; c->claim_cas += sum; c->claim_pr += pr; }
            continue;
        }
    }
    if (!have_bpr) { si = st; RFAIL(RM_ERR_STRUCTURE, "no BPR segment"); }
    for (int i = 0; i < r->n_claim; i++) {
        rm_claim_t *cc = &r->claim[i]; int64_t svc_cas = 0, svc_pr = 0; int lines_ok = 1;
        for (int k = 0; k < cc->n_line; k++) { rm_line_t *l = &cc->line[k]; l->balanced = (l->charge - l->paid == l->cas_sum); if (!l->balanced) { lines_ok = 0; } svc_cas += l->cas_sum; svc_pr += l->pr_sum; }
        int64_t expect = cc->has_claim_cas ? cc->claim_cas : svc_cas; int64_t prsum = cc->claim_pr + svc_pr;
        cc->balanced = (cc->charge - cc->paid == expect) && lines_ok; cc->pr_matches = (cc->patient == prsum);
        if (!cc->balanced) { r->all_balanced = 0; }
    }
    r->bpr_balanced = (r->bpr_total == r->paid_sum - r->plb_sum); if (!r->bpr_balanced) { r->all_balanced = 0; }
    return RM_OK;
}
