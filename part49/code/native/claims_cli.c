/* Chapter 49 host tool, built from the SAME source files the kernel links.
 *   claims_cli survey FILE             envelope + 837 verdict (one line)
 *   claims_cli adj FILE [d dm cp bp oop oopm]   adjudicate every claim in order, print the canonical text (what adjud_ref.py prints)
 *   claims_cli build FILE [...]        the 835 for those claims
 *   claims_cli recon FILE              parse an 835 and print the reconciliation (canonical text of remit_ref.py) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../049_x12.h"
#include "../049_claim.h"
#include "../049_adjud.h"
#include "../049_remit.h"
static x12_t X; static cl_batch_t B; static adj_state_t S; static adj_claim_t R[CL_MAX_CLAIM]; static rm_report_t RM; static char buf[1 << 15]; static char txt[1 << 22];
static void money(char *o, int64_t c) { x12_money_str(c, o); }
int main(int argc, char **argv) {
    if (argc < 3) { return 2; }
    FILE *f = fopen(argv[2], "rb"); if (!f) { perror("open"); return 2; } size_t n = fread(txt, 1, sizeof txt - 1, f); fclose(f);
    X.lenient = getenv("LENIENT") != NULL; int rc = x12_parse(&X, txt, (uint32_t)n);
    if (!strcmp(argv[1], "recon")) {
        if (rc) { printf("X12 %d %s (segment %d)\n", rc, x12_strerror(rc), X.err_seg); return 1; }
        rc = rm_parse(&RM, &X); if (rc) { printf("835 %d %s (segment %d)\n", rc, RM.msg, RM.err_seg); return 1; }
        char a[28], b2[28];
        money(a, RM.bpr_total); money(b2, RM.paid_sum); printf("bpr %s clp_paid_sum %s plb %s balanced %d\n", a, b2, RM.has_plb ? "yes" : "no", RM.bpr_balanced);
        for (int i = 0; i < RM.n_claim; i++) { const rm_claim_t *c = &RM.claim[i]; money(a, c->charge); money(b2, c->paid); char p[28]; money(p, c->patient); printf("claim %s status %d charge %s paid %s patient %s lines %d balanced %d pr_matches %d\n", c->pcn, c->status, a, b2, p, c->n_line, c->balanced, c->pr_matches);
            for (int k = 0; k < c->n_line; k++) { char l1[28], l2[28], l3[28]; money(l1, c->line[k].charge); money(l2, c->line[k].paid); money(l3, c->line[k].cas_sum); printf("  line %d charge %s paid %s cas %s balanced %d\n", k + 1, l1, l2, l3, c->line[k].balanced); } }
        printf("all_balanced %d\n", RM.all_balanced); return 0;
    }
    if (rc) { printf("X12 %d %s (segment %d)\n", rc, x12_strerror(rc), X.err_seg); return 1; }
    rc = cl_parse(&B, &X); if (rc) { printf("837 %d %s: %s (segment %d)\n", rc, cl_strerror(rc), B.msg, B.err_seg); return 1; }
    if (!strcmp(argv[1], "survey")) { printf("OK %d claims", B.n_claim); for (int i = 0; i < B.n_claim; i++) { printf(" [%s %d lines]", B.claim[i].pcn, B.claim[i].n_line); } printf("\n"); return 0; }
    adj_plan_t p; adj_default_plan(&p);
    if (argc >= 9) { p.deductible = (uint32_t)atol(argv[3]); p.ded_met = (uint32_t)atol(argv[4]); p.copay = (uint32_t)atol(argv[5]); p.coins_bp = (uint32_t)atol(argv[6]); p.oop_max = (uint32_t)atol(argv[7]); p.oop_met = (uint32_t)atol(argv[8]); }
    adj_init(&S, &p); static char out[1 << 16]; uint32_t o = 0;
    for (int i = 0; i < B.n_claim; i++) { int r = adj_claim(&S, &B.claim[i], &R[i]); if (r) { printf("ADJ_ERROR %d\n", r); return 1; } int k = adj_canonical(&R[i], &B.claim[i], &S, out + o, sizeof out - o); if (k < 0) { return 3; } o += (uint32_t)k; }
    if (!strcmp(argv[1], "adj")) { fputs(out, stdout); return 0; }
    rm_hdr_t h; memset(&h, 0, sizeof h); strcpy(h.payer_id, B.claim[0].payer_id); strcpy(h.payer_name, B.claim[0].payer_name); strcpy(h.payee_npi, B.claim[0].bill_npi); strcpy(h.payee_name, B.claim[0].bill_name); h.date = 19000; h.ctl = 1;
    int len = rm_build(buf, sizeof buf, &h, B.claim, R, B.n_claim); if (len < 0) { return 4; } fputs(buf, stdout); return 0;
}
