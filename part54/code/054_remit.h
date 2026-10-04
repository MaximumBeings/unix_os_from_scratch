/* Chapter 49: the 835 remittance advice (ASC X12N 835, version 005010X221A1): what the payer sends back. It lists each claim with what was billed (CLP03), what was paid (CLP04) and what the patient owes (CLP05), and each service line (SVC) with
 * the adjustments (CAS) that explain every dollar that was not paid. The BUILDER turns this chapter's adjudication results into an 835. The READER parses any 835 and RECONCILES it -- the check a provider's billing office runs on every real
 * remittance, because a remittance whose numbers do not add up cannot be posted:
 *   * for each service line:   SVC02 (charge) - SVC03 (paid)  =  the sum of that line's CAS amounts;
 *   * for each claim:          CLP03 (charge) - CLP04 (paid)  =  the sum of the claim-level CAS amounts if the claim has any, else the sum over its service lines;
 *   * for the whole 835:       BPR02 (the payment)            =  the sum of the CLP04 amounts less the provider-level adjustments (PLB);
 *   * information only:        CLP05 (patient responsibility) =  the sum of the PR (patient responsibility) amounts.
 * Amounts in an 835 can be negative (a reversal): adjustments are signed. */
#ifndef REMIT_H
#define REMIT_H
#include <stdint.h>
#include "054_claim.h"
#include "054_adjud.h"

#define RM_MAX_CLAIM 16
#define RM_MAX_LINE 16
typedef struct { char payer_id[26], payer_name[40], payee_npi[11], payee_name[40]; int32_t date; uint32_t ctl; /* production date (days since 1970-01-01), interchange control number */ } rm_hdr_t;
int rm_build(char *buf, uint32_t cap, const rm_hdr_t *h, const cl_claim_t *claims, const adj_claim_t *res, int n); /* the 835 text; its length, or -1 if cap is too small */

typedef struct { int64_t charge, paid, cas_sum, pr_sum; uint8_t n_cas, balanced; } rm_line_t;
typedef struct { char pcn[40]; int status; int64_t charge, paid, patient, claim_cas, claim_pr; uint8_t has_claim_cas, n_line, balanced, pr_matches; rm_line_t line[RM_MAX_LINE]; } rm_claim_t;
typedef struct { int64_t bpr_total, plb_sum, paid_sum; uint8_t n_claim, has_plb, bpr_balanced, all_balanced; rm_claim_t claim[RM_MAX_CLAIM]; uint16_t err_seg; const char *msg; } rm_report_t;
enum { RM_OK = 0, RM_ERR_NOT_835 = -1, RM_ERR_STRUCTURE = -2, RM_ERR_ELEMENT = -3, RM_ERR_LIMIT = -4 };
int rm_parse(rm_report_t *r, const x12_t *x); /* parses the first 835 and fills in the reconciliation; RM_OK even when it does not balance (look at all_balanced) */
void rm_date_str(int32_t days, char *out9); /* CCYYMMDD */
#endif
