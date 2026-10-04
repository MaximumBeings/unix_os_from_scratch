#!/bin/bash
# Chapter 52: every host-side test, the C built with AddressSanitizer and UBSan, in one run. Writes everything to stdout (the chapter page shows it as host_tests_out.txt).
cd "$(dirname "$0")"; F="-O1 -fsanitize=address,undefined -fno-sanitize-recover=undefined -Wall -Wextra"; SRC="../052_payroll.c"
echo "== 1. the engine, worked by hand =="
gcc $F pay_test.c $SRC -o /tmp/c52_pt 2>&1 | grep -i error; /tmp/c52_pt | grep -v "^PASS"; /tmp/c52_pt | grep -c "^PASS" | sed 's/$/ lines PASS/'
echo; echo "== 2. 60 random companies: C vs the independent Python reference, every cent =="
gcc $F pay_cli.c $SRC -o /tmp/c52_cli; python3 diff_pay.py 60 /tmp/c52_cli
echo; echo "== 3. random pay periods (ordinary and extreme values), invariants after every call =="
gcc $F pay_fuzz.c $SRC -o /tmp/c52_fz && /tmp/c52_fz 3000
