#!/bin/bash
# Chapter 49: every host-side test, the C built with AddressSanitizer and UBSan, in one run. Writes everything to stdout (the chapter page shows it as host_tests_out.txt).
cd "$(dirname "$0")"; F="-O1 -fsanitize=address,undefined -fno-sanitize-recover=undefined -Wall -Wextra"; SRC="../049_x12.c ../049_claim.c ../049_adjud.c ../049_remit.c"
echo "== 1. the X12 reader, the claim reader, the adjudication engine and the 835 builder/reconciler: worked by hand =="
gcc $F claims_test.c $SRC -o /tmp/c49_ct 2>&1 | grep -i error; /tmp/c49_ct | grep -v "^PASS"; /tmp/c49_ct | grep -c "^PASS" | sed 's/$/ lines PASS/'
echo; echo "== 2. the ten real-format open-source files, read by the same C files the kernel links =="
gcc $F claims_cli.c $SRC -o /tmp/c49_cli
for f in ../data/real/x12_valid.txt ../data/real/x12_no_errors.txt ../data/real/x12_complex.txt ../data/real/x12_valid_different_separators.txt ../data/real/x12_missing_elements.txt ../data/real/x12_bad_segment_identifier.txt ../data/real/x12_bad_first_line.txt ../data/real/x12_ambiguous_loop.txt ../data/real/x12_999_accepted.txt; do
  printf "%-36s strict:  %s\n" "$(basename $f)" "$(/tmp/c49_cli survey $f | cut -c1-110)"; printf "%-36s lenient: %s\n" "" "$(LENIENT=1 /tmp/c49_cli survey $f | cut -c1-150)"; done
echo "835_mult_loops.txt reconciliation:"; /tmp/c49_cli recon ../data/real/835_mult_loops.txt | sed 's/^/   /'
echo; echo "== 3. 400 random valid claim sets: C engine vs the independent Python reference (adjudication, the 835 each builds, its reconciliation, and tampered 835s) =="
python3 diff_claims.py 400 /tmp/c49_cli
echo; echo "== 4. damaged claims and remittances: 3,000 mutations of each of 9 files, strict and lenient, sanitizers on =="
for n in A B C D E F; do /tmp/c49_cli build ../data/claim_$n.837 > /tmp/c49_835_$n.835; done
gcc $F claims_fuzz.c $SRC -o /tmp/c49_fz && /tmp/c49_fz 3000 ../data/claim_A.837 ../data/claim_E.837 ../data/real/x12_valid.txt ../data/real/x12_complex.txt ../data/real/835_mult_loops.txt /tmp/c49_835_A.835 /tmp/c49_835_B.835 /tmp/c49_835_D.835 /tmp/c49_835_E.835
