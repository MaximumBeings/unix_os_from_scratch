#!/bin/bash
# Chapter 50: every host-side test, the C built with AddressSanitizer and UBSan, in one run. Writes everything to stdout (the chapter page shows it as host_tests_out.txt).
cd "$(dirname "$0")"; F="-O1 -fsanitize=address,undefined -fno-sanitize-recover=undefined -Wall -Wextra"; SRC="../050_btc.c ../050_sha256.c"
echo "== 1. the validator: hashing, the compact target, real blocks against their published hashes, Core's transaction vectors, every rule, damage =="
gcc $F btc_test.c $SRC -o /tmp/c50_bt 2>&1 | grep -i error; /tmp/c50_bt | grep -v "^PASS"; /tmp/c50_bt | grep -c "^PASS" | sed 's/$/ lines PASS/'
echo; echo "== 2. 400 random blocks (valid, SegWit, and 17 kinds of defect), then every data/btc block file and every Core transaction vector: C vs the independent Python reference =="
gcc $F btc_cli.c $SRC -o /tmp/c50_cli; python3 diff_btc.py 400 /tmp/c50_cli
echo; echo "== 3. damaged blocks: 6,000 mutations of each of the 24 embedded blocks, sanitizers on: a changed block must never be valid =="
gcc $F btc_fuzz.c $SRC -o /tmp/c50_fz && /tmp/c50_fz 6000 ../data/btc/*.blk
