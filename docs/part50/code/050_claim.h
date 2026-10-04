/* Chapter 49: the 837 Professional claim reader (ASC X12N 837P, version 005010X222A1). Takes a parsed X12 interchange (050_x12.h) and extracts, for each claim, the fields adjudication needs -- the billing provider and its NPI, the subscriber and
 * member id, the payer, the patient control number, the total charge, the diagnoses, and each service line (procedure code, charge, units, date of service) -- while checking the rules that make a claim worth adjudicating at all:
 *   * the NPI (National Provider Identifier) has a valid CHECK DIGIT (a Luhn checksum over the nine digits with the prefix 80840 -- a rule the standard publishes, so a mistyped NPI is caught before any money moves);
 *   * HL (hierarchical level) numbers are 1, 2, 3, ... in order and every child names the right parent;
 *   * service lines are numbered 1, 2, 3, ... and every date is a real calendar date;
 *   * the claim's total charge (CLM02) EQUALS the sum of its service-line charges (SV102) -- the balancing rule every payer applies first;
 *   * every amount is a plain decimal with at most two places, units are whole numbers.
 * It is an EXTRACTING reader, not a full implementation-guide validator: segments it does not need (N3, N4, PER, PRV, REF, DMG, ...) are accepted without inspection, and only the claim shapes this chapter supports are accepted (patient is the
 * subscriber; professional claims; ICD-10 or ICD-9 principal diagnosis). Anything else is refused with a reason, never half-read. */
#ifndef CLAIM_H
#define CLAIM_H
#include <stdint.h>
#include "050_x12.h"

#define CL_MAX_CLAIM 8
#define CL_MAX_LINE 16
#define CL_MAX_DX 12

enum { CL_OK = 0, CL_ERR_X12 = -1, CL_ERR_NOT_837 = -2, CL_ERR_VERSION = -3, CL_ERR_BHT = -4, CL_ERR_HL = -5, CL_ERR_UNSUPPORTED = -6, CL_ERR_NPI = -7, CL_ERR_ELEMENT = -8, CL_ERR_STRUCTURE = -9, CL_ERR_BALANCE = -10,
       CL_ERR_LIMIT = -11 };

typedef struct { char code[6]; char mod[3]; int64_t charge; uint32_t units; int32_t dos; uint8_t dx_ptr; } cl_line_t; /* dx_ptr: 1-based pointer to the first diagnosis, 0 if none */
typedef struct {
    char pcn[40]; int64_t total; char pos[3]; char dx[CL_MAX_DX][9]; uint8_t n_dx; uint8_t n_line; cl_line_t line[CL_MAX_LINE];
    char sub_id[26], sub_last[36], sub_first[26], payer_id[26], payer_name[40], bill_npi[11], bill_name[40];
} cl_claim_t;
typedef struct { uint8_t n_claim; cl_claim_t claim[CL_MAX_CLAIM]; uint16_t err_seg; const char *msg; } cl_batch_t;

int cl_parse(cl_batch_t *b, const x12_t *x); /* CL_OK or a negative CL_ERR_*; b->err_seg and b->msg say where and why */
int cl_npi_valid(const char *npi10); /* 1 when the ten digits pass the Luhn check with prefix 80840 */
const char *cl_strerror(int rc);
#endif
