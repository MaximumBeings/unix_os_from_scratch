#!/bin/bash
# Chapter 53: every host-side test, the C built with AddressSanitizer and UBSan, in one run. Writes everything to stdout (the chapter page shows it as host_tests_out.txt).
cd "$(dirname "$0")"; F="-O1 -fsanitize=address,undefined -fno-sanitize-recover=undefined -Wall -Wextra"; SRC="../053_lsm.c ../053_sha256.c"
echo "== 1. the store, worked by hand =="
gcc $F lsm_test.c $SRC -o /tmp/c53_t 2>&1 | grep -i error; /tmp/c53_t | grep -v "^PASS" | grep -v "^$"; /tmp/c53_t | grep -c "^PASS" | sed 's/$/ lines PASS/'
echo; echo "== 2. 60 random scripts with crashes at random bytes: C vs the independent Python store, every result and every byte of every file =="
gcc $F lsm_cli.c $SRC -o /tmp/c53_cli; python3 diff_lsm.py 60 /tmp/c53_cli
echo; echo "== 3. the crash test: a crash at EVERY byte of a 120-operation workload, and again at every byte of recovery for every 40th =="
gcc $F lsm_crash.c $SRC -o /tmp/c53_cr && /tmp/c53_cr 7 120 1 40
echo; echo "== 4. a second workload (seed 21, 300 operations, every 11th byte; every 3rd of those also crashed during recovery) =="
/tmp/c53_cr 21 300 11 3
echo; echo "== 5. random operations against a model, with byte flips =="
gcc $F lsm_fuzz.c $SRC -o /tmp/c53_fz && /tmp/c53_fz 12
