/* Chapter 49: the adjudication engine (see 053_adjud.h for the rules). */
#include "053_adjud.h"

static uint64_t udm(uint64_t n, uint64_t dv, uint64_t *rem) { /* shift-and-subtract division: no libgcc in this kernel */
    uint64_t q = 0, r = 0;
    for (int i = 63; i >= 0; i--) { r = (r << 1) | ((n >> i) & 1u); if (r >= dv) { r -= dv; q |= 1ull << i; } }
    *rem = r; return q;
}
static const struct { const char *code; uint32_t fee; uint8_t em; } FEES[] = { /* invented allowed amounts per unit, in cents; the codes are standard CPT/HCPCS numbers, the amounts are not anyone's contract */
    {"99202", 9000, 1}, {"99203", 13500, 1}, {"99204", 20500, 1}, {"99205", 26500, 1}, {"99211", 2500, 1}, {"99212", 5500, 1}, {"99213", 8800, 1}, {"99214", 12500, 1}, {"99215", 17500, 1},
    {"85025", 1500, 0}, {"80053", 2000, 0}, {"36415", 300, 0}, {"71046", 4500, 0}, {"93000", 3500, 0}, {"45378", 38000, 0}, {"29881", 120000, 0}, {"27447", 1500000, 0},
};
#define N_FEES ((int)(sizeof(FEES) / sizeof(FEES[0])))
static int str_eq(const char *a, const char *b) { uint32_t i = 0; while (a[i] && a[i] == b[i]) { i++; } return a[i] == b[i]; }
int adj_fee(const char *code, uint32_t *fee, int *is_em) {
    for (int i = 0; i < N_FEES; i++) { if (str_eq(FEES[i].code, code)) { *fee = FEES[i].fee; *is_em = FEES[i].em; return 1; } }
    return 0;
}
void adj_default_plan(adj_plan_t *p) { p->deductible = 50000; p->ded_met = 0; p->copay = 2500; p->coins_bp = 2000; p->oop_max = 200000; p->oop_met = 0; }
void adj_init(adj_state_t *s, const adj_plan_t *plan) { s->plan = *plan; s->n_hist = 0; }

static void add_adj(adj_line_t *l, int group, uint16_t carc, int64_t amount) {
    if (amount == 0) { return; }
    adj_item_t *a = &l->adj[l->n_adj++]; a->group = (uint8_t)group; a->carc = carc; a->amount = amount;
}
static int is_dup(const adj_state_t *s, const cl_claim_t *c, const cl_line_t *ln) {
    for (uint16_t i = 0; i < s->n_hist; i++) {
        const adj_hist_t *h = &s->hist[i];
        if (str_eq(h->pcn, c->pcn) && str_eq(h->code, ln->code) && h->dos == ln->dos && h->charge == ln->charge) { return 1; }
    }
    return 0;
}
int adj_claim(adj_state_t *s, const cl_claim_t *c, adj_claim_t *out) {
    int32_t em_dos[CL_MAX_LINE]; int n_em = 0;
    if ((uint32_t)s->n_hist + c->n_line > ADJ_MAX_HIST) { return ADJ_ERR_HISTORY_FULL; } /* refuse BEFORE changing any state */
    out->n_line = c->n_line; out->total_charge = out->total_paid = out->total_pr = out->total_co = 0;
    int all_denied = 1;
    for (int i = 0; i < c->n_line; i++) {
        const cl_line_t *ln = &c->line[i]; adj_line_t *l = &out->line[i];
        l->charge = ln->charge; l->allowed = 0; l->paid = 0; l->denied = 0; l->n_adj = 0;
        uint32_t fee; int em;
        if (is_dup(s, c, ln)) { l->denied = 1; add_adj(l, ADJ_CO, 18, ln->charge); }
        else if (!adj_fee(ln->code, &fee, &em)) { l->denied = 1; add_adj(l, ADJ_CO, 96, ln->charge); }
        else {
            all_denied = 0;
            int64_t cap = (int64_t)fee * (int64_t)ln->units; int64_t allowed = ln->charge < cap ? ln->charge : cap; l->allowed = allowed;
            add_adj(l, ADJ_CO, 45, ln->charge - allowed);
            int64_t cp = 0;
            if (em) { int seen = 0; for (int k = 0; k < n_em; k++) { if (em_dos[k] == ln->dos) { seen = 1; } } if (!seen) { em_dos[n_em++] = ln->dos; cp = (int64_t)s->plan.copay < allowed ? (int64_t)s->plan.copay : allowed; } }
            int64_t ded_left = (int64_t)s->plan.deductible - (int64_t)s->plan.ded_met; if (ded_left < 0) { ded_left = 0; }
            int64_t after_cp = allowed - cp; int64_t d = ded_left < after_cp ? ded_left : after_cp;
            int64_t base = after_cp - d; uint64_t rem; int64_t coins = (int64_t)udm((uint64_t)base * (uint64_t)s->plan.coins_bp + 5000u, 10000u, &rem);
            int64_t room = (int64_t)s->plan.oop_max - (int64_t)s->plan.oop_met; if (room < 0) { room = 0; }
            int64_t pr = cp + d + coins;
            if (pr > room) {
                int64_t excess = pr - room; int64_t cut = excess < coins ? excess : coins; coins -= cut; excess -= cut;
                cut = excess < d ? excess : d; d -= cut; excess -= cut;
                cut = excess < cp ? excess : cp; cp -= cut; excess -= cut;
            }
            pr = cp + d + coins; l->paid = allowed - pr;
            add_adj(l, ADJ_PR, 3, cp); add_adj(l, ADJ_PR, 1, d); add_adj(l, ADJ_PR, 2, coins);
            s->plan.ded_met += (uint32_t)d; s->plan.oop_met += (uint32_t)pr;
        }
        if (s->n_hist >= ADJ_MAX_HIST) { return ADJ_ERR_HISTORY_FULL; }
        { adj_hist_t *h = &s->hist[s->n_hist++]; for (int k = 0; k < 40; k++) { h->pcn[k] = c->pcn[k]; } for (int k = 0; k < 6; k++) { h->code[k] = ln->code[k]; } h->dos = ln->dos; h->charge = ln->charge; }
        out->total_charge += l->charge; out->total_paid += l->paid;
        for (int k = 0; k < l->n_adj; k++) { if (l->adj[k].group == ADJ_PR) { out->total_pr += l->adj[k].amount; } else { out->total_co += l->adj[k].amount; } }
    }
    out->status = all_denied ? 4 : 1;
    return adj_verify(s, c, out) == 0 ? ADJ_OK : ADJ_ERR_INVARIANT;
}

const char *adj_verify_msg(int code) {
    switch (code) {
    case 0: return "ok"; case 1: return "a line's charge is not paid + CO + PR"; case 2: return "a negative paid amount or adjustment"; case 3: return "paid exceeds allowed"; case 4: return "the claim totals do not add up";
    case 5: return "the deductible met passed the deductible"; case 6: return "the out-of-pocket met passed the maximum"; case 7: return "claim charge differs from the 837 total";
    }
    return "unknown";
}
int adj_verify(const adj_state_t *s, const cl_claim_t *c, const adj_claim_t *r) {
    int64_t tc = 0, tp = 0, tpr = 0, tco = 0;
    for (int i = 0; i < r->n_line; i++) {
        const adj_line_t *l = &r->line[i]; int64_t sum = l->paid;
        if (l->paid < 0) { return 2; }
        if (l->paid > l->allowed) { return 3; }
        for (int k = 0; k < l->n_adj; k++) { if (l->adj[k].amount <= 0) { return 2; } sum += l->adj[k].amount; if (l->adj[k].group == ADJ_PR) { tpr += l->adj[k].amount; } else { tco += l->adj[k].amount; } }
        if (sum != l->charge) { return 1; }
        tc += l->charge; tp += l->paid;
    }
    if (tc != r->total_charge || tp != r->total_paid || tpr != r->total_pr || tco != r->total_co || tc != tp + tpr + tco) { return 4; }
    if (tc != c->total) { return 7; }
    if (s->plan.ded_met > s->plan.deductible) { return 5; }
    if (s->plan.oop_met > s->plan.oop_max) { return 6; }
    return 0;
}

static int put_s(char *b, uint32_t cap, uint32_t *o, const char *s) { while (*s) { if (*o + 1 >= cap) { return -1; } b[(*o)++] = *s++; } return 0; }
static int put_i(char *b, uint32_t cap, uint32_t *o, int64_t v) {
    char t[24]; int n = 0; uint64_t m = (uint64_t)(v < 0 ? -v : v), r; if (m == 0) { t[n++] = '0'; } while (m) { m = udm(m, 10, &r); t[n++] = (char)('0' + (int)r); }
    if (v < 0 && put_s(b, cap, o, "-")) { return -1; }
    while (n) { char one[2] = {t[--n], 0}; if (put_s(b, cap, o, one)) { return -1; } }
    return 0;
}
int adj_canonical(const adj_claim_t *r, const cl_claim_t *c, const adj_state_t *s, char *buf, uint32_t cap) {
    uint32_t o = 0;
    if (put_s(buf, cap, &o, "claim ") || put_s(buf, cap, &o, c->pcn) || put_s(buf, cap, &o, " status ") || put_i(buf, cap, &o, r->status) || put_s(buf, cap, &o, " charge ") || put_i(buf, cap, &o, r->total_charge) ||
        put_s(buf, cap, &o, " paid ") || put_i(buf, cap, &o, r->total_paid) || put_s(buf, cap, &o, " pr ") || put_i(buf, cap, &o, r->total_pr) || put_s(buf, cap, &o, " co ") || put_i(buf, cap, &o, r->total_co) || put_s(buf, cap, &o, "\n")) { return -1; }
    for (int i = 0; i < r->n_line; i++) {
        const adj_line_t *l = &r->line[i];
        if (put_s(buf, cap, &o, "  line ") || put_i(buf, cap, &o, i + 1) || put_s(buf, cap, &o, " ") || put_s(buf, cap, &o, c->line[i].code) || put_s(buf, cap, &o, " charge ") || put_i(buf, cap, &o, l->charge) ||
            put_s(buf, cap, &o, " allowed ") || put_i(buf, cap, &o, l->allowed) || put_s(buf, cap, &o, " paid ") || put_i(buf, cap, &o, l->paid)) { return -1; }
        for (int k = 0; k < l->n_adj; k++) { if (put_s(buf, cap, &o, l->adj[k].group == ADJ_PR ? " PR-" : " CO-") || put_i(buf, cap, &o, l->adj[k].carc) || put_s(buf, cap, &o, "=") || put_i(buf, cap, &o, l->adj[k].amount)) { return -1; } }
        if (put_s(buf, cap, &o, "\n")) { return -1; }
    }
    if (put_s(buf, cap, &o, "  accumulators deductible_met ") || put_i(buf, cap, &o, s->plan.ded_met) || put_s(buf, cap, &o, " oop_met ") || put_i(buf, cap, &o, s->plan.oop_met) || put_s(buf, cap, &o, "\n")) { return -1; }
    buf[o] = 0; return (int)o;
}
