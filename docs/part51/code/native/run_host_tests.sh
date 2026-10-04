#!/bin/bash
# Chapter 51: every host-side test, the C built with AddressSanitizer and UBSan, in one run. Writes everything to stdout (the chapter page shows it as host_tests_out.txt).
cd "$(dirname "$0")"; F="-O1 -fsanitize=address,undefined -fno-sanitize-recover=undefined -Wall -Wextra"; SRC="../051_book.c ../051_sha256.c"
echo "== 1. the engine and the subscriber, worked by hand =="
gcc $F book_test.c $SRC -o /tmp/c51_bt 2>&1 | grep -i error; /tmp/c51_bt | grep -v "^PASS"; /tmp/c51_bt | grep -c "^PASS" | sed 's/$/ lines PASS/'
echo; echo "== 2. 60 random order flows (60 / 300 / 700 commands) and 3 damaged feeds of each: C vs the independent Python engine, subscriber and hash =="
gcc $F book_cli.c $SRC -o /tmp/c51_cli; python3 diff_book.py 60 /tmp/c51_cli
echo; echo "== 3. random API calls (2,000 per run, ordinary and extreme values), invariants after every call, feeds rebuilt and damaged =="
gcc $F book_fuzz.c $SRC -o /tmp/c51_fz && /tmp/c51_fz 12
