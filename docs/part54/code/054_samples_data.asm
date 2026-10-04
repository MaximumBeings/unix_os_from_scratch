; Chapter 49: the files the demo reads, baked into the kernel image with NASM's incbin (paths relative to the chapter's code directory, where build.sh runs).
;   data/real/*  open-source X12 test files, copied unchanged (see data/real/PROVENANCE.txt)
;   data/claim_*.837  this chapter's own INVENTED claims (made by make_claims.py)
BITS 32
section .rodata
%macro SAMPLE 2
global sample_%1_start
global sample_%1_end
sample_%1_start: incbin %2
sample_%1_end:
%endmacro
SAMPLE r_valid, "data/real/x12_valid.txt"
SAMPLE r_noerr, "data/real/x12_no_errors.txt"
SAMPLE r_complex, "data/real/x12_complex.txt"
SAMPLE r_seps, "data/real/x12_valid_different_separators.txt"
SAMPLE r_missing, "data/real/x12_missing_elements.txt"
SAMPLE r_badseg, "data/real/x12_bad_segment_identifier.txt"
SAMPLE r_badfirst, "data/real/x12_bad_first_line.txt"
SAMPLE r_ambig, "data/real/x12_ambiguous_loop.txt"
SAMPLE r_999, "data/real/x12_999_accepted.txt"
SAMPLE r_835, "data/real/835_mult_loops.txt"
SAMPLE c_a, "data/claim_A.837"
SAMPLE c_b, "data/claim_B.837"
SAMPLE c_c, "data/claim_C.837"
SAMPLE c_d, "data/claim_D.837"
SAMPLE c_e, "data/claim_E.837"
SAMPLE c_f, "data/claim_F.837"
