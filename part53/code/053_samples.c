/* Chapter 49: the sample table. */
#include "053_samples.h"
extern const char sample_r_valid_start[], sample_r_valid_end[], sample_r_noerr_start[], sample_r_noerr_end[], sample_r_complex_start[], sample_r_complex_end[], sample_r_seps_start[], sample_r_seps_end[];
extern const char sample_r_missing_start[], sample_r_missing_end[], sample_r_badseg_start[], sample_r_badseg_end[], sample_r_badfirst_start[], sample_r_badfirst_end[], sample_r_ambig_start[], sample_r_ambig_end[];
extern const char sample_r_999_start[], sample_r_999_end[], sample_r_835_start[], sample_r_835_end[];
extern const char sample_c_a_start[], sample_c_a_end[], sample_c_b_start[], sample_c_b_end[], sample_c_c_start[], sample_c_c_end[], sample_c_d_start[], sample_c_d_end[], sample_c_e_start[], sample_c_e_end[], sample_c_f_start[], sample_c_f_end[];
const sample_t g_real[N_REAL] = {
    {"x12_valid.txt", "837P, one claim", sample_r_valid_start, sample_r_valid_end}, {"x12_no_errors.txt", "837P, two claims", sample_r_noerr_start, sample_r_noerr_end},
    {"x12_complex.txt", "837P, many segment kinds", sample_r_complex_start, sample_r_complex_end}, {"x12_valid_different_separators.txt", "837P, '~' and '_' as separators", sample_r_seps_start, sample_r_seps_end},
    {"x12_missing_elements.txt", "837P, elements left out on purpose", sample_r_missing_start, sample_r_missing_end}, {"x12_bad_segment_identifier.txt", "837P, a bad segment identifier on purpose", sample_r_badseg_start, sample_r_badseg_end},
    {"x12_bad_first_line.txt", "837P, a damaged ISA on purpose", sample_r_badfirst_start, sample_r_badfirst_end}, {"x12_ambiguous_loop.txt", "837P, an ambiguous loop on purpose", sample_r_ambig_start, sample_r_ambig_end},
    {"x12_999_accepted.txt", "999 acknowledgement", sample_r_999_start, sample_r_999_end}, {"835_mult_loops.txt", "835 remittance, three denied lines", sample_r_835_start, sample_r_835_end},
};
const sample_t g_claims[N_CLAIMS] = {
    {"claim_A.837", "A: office visit, blood count, blood draw", sample_c_a_start, sample_c_a_end}, {"claim_B.837", "B: a longer visit and an ECG", sample_c_b_start, sample_c_b_end},
    {"claim_C.837", "C: claim A sent again", sample_c_c_start, sample_c_c_end}, {"claim_D.837", "D: a screening colonoscopy and a supply that is not covered", sample_c_d_start, sample_c_d_end},
    {"claim_E.837", "E: a knee replacement", sample_c_e_start, sample_c_e_end}, {"claim_F.837", "F: a visit after the out-of-pocket maximum", sample_c_f_start, sample_c_f_end},
};
