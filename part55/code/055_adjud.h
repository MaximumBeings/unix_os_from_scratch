/* Chapter 49: the adjudication engine. Adjudication is the payer's decision on each service line of a claim: what was billed, what the plan ALLOWS for that service, what the PLAN pays, what the PATIENT owes, and -- for every dollar that is
 * neither -- WHY, as a standard code. The reasons are X12's Claim Adjustment Reason Codes (CARC), grouped into the two groups the 835 remittance uses:
 *   CO (contractual obligation, the provider must write it off): 45 charge exceeds the fee schedule, 18 exact duplicate claim or service, 96 non-covered charge;
 *   PR (patient responsibility, the patient owes it): 3 co-payment, 1 deductible, 2 coinsurance.
 * THE RULES (invented plan, real mechanics -- real fee schedules and plan documents are proprietary, so the numbers below are this chapter's own and the chapter says so):
 *   per line, in this order:
 *   1. a line with the same patient control number, procedure code, date of service and charge as one already adjudicated is a DUPLICATE: nothing is paid, CO-18 for the whole charge;
 *   2. a procedure code that is not in the fee schedule is NON-COVERED: nothing is paid, CO-96 for the whole charge;
 *   3. the ALLOWED amount is the smaller of the charge and (fee for one unit x units); the rest of the charge is CO-45;
 *   4. the CO-PAY (office visits only, the evaluation-and-management codes) applies once per date of service in a claim, to the first such line: PR-3, never more than the allowed amount;
 *   5. the DEDUCTIBLE takes what remains of the year's deductible from what is left of the allowed amount: PR-1;
 *   6. COINSURANCE is the patient's share (basis points) of what is still left, rounded half up to the cent: PR-2;
 *   7. the OUT-OF-POCKET MAXIMUM caps the patient's total for the year: if co-pay + deductible + coinsurance would pass it, the excess is taken off coinsurance first, then deductible, then co-pay;
 *   8. the PLAN PAYS the allowed amount less the patient's share.
 * Every amount is an integer number of cents. For every line the engine guarantees: charge = paid + (all CO) + (all PR), exactly. */
#ifndef ADJUD_H
#define ADJUD_H
#include <stdint.h>
#include "055_claim.h"

#define ADJ_MAX_HIST 128
#define ADJ_MAX_ADJ 4
enum { ADJ_OK = 0, ADJ_ERR_HISTORY_FULL = -1, ADJ_ERR_INVARIANT = -2 };
enum { ADJ_CO = 0, ADJ_PR = 1 };
typedef struct { uint32_t deductible, ded_met, copay, coins_bp, oop_max, oop_met; } adj_plan_t; /* money in cents; coins_bp: the PATIENT's share of coinsurance, basis points (2000 = 20.00%) */
typedef struct { char pcn[40]; char code[6]; int32_t dos; int64_t charge; } adj_hist_t;
typedef struct { adj_plan_t plan; uint16_t n_hist; adj_hist_t hist[ADJ_MAX_HIST]; } adj_state_t;
typedef struct { uint8_t group; uint16_t carc; int64_t amount; } adj_item_t;
typedef struct { int64_t charge, allowed, paid; uint8_t denied, n_adj; adj_item_t adj[ADJ_MAX_ADJ]; } adj_line_t;
typedef struct { uint8_t n_line, status; /* 1 processed as primary, 4 denied */ int64_t total_charge, total_paid, total_pr, total_co; adj_line_t line[CL_MAX_LINE]; } adj_claim_t;

void adj_default_plan(adj_plan_t *p); /* $500 deductible, $25 co-pay, 20% coinsurance, $2,000 out-of-pocket maximum, nothing met yet */
void adj_init(adj_state_t *s, const adj_plan_t *plan);
int adj_claim(adj_state_t *s, const cl_claim_t *c, adj_claim_t *out); /* ADJ_OK; the engine's own invariants are checked before returning */
int adj_fee(const char *code, uint32_t *fee_cents, int *is_em); /* the fee schedule: 1 if the code is covered */
int adj_verify(const adj_state_t *s, const cl_claim_t *c, const adj_claim_t *r); /* 0 when every invariant holds; a positive number names the first one that does not */
const char *adj_verify_msg(int code);
int adj_canonical(const adj_claim_t *r, const cl_claim_t *c, const adj_state_t *s, char *buf, uint32_t cap); /* the text adjud_ref.py prints for one claim; returns its length or -1 */
#endif
