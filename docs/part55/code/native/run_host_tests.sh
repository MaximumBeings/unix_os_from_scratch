#!/bin/bash
# Chapter 55: every host-side test, the C built with AddressSanitizer and UBSan, in one run. Writes everything to stdout (the chapter page shows it as host_tests_out.txt).
cd "$(dirname "$0")"; F="-O1 -fsanitize=address,undefined -fno-sanitize-recover=undefined -Wall -Wextra"; SRC="../055_raft.c ../055_sim.c"
echo "== 1. the node, driven by hand, including Figures 7 and 8 of the Raft paper =="
gcc $F raft_test.c ../055_raft.c -o /tmp/c55_t 2>&1 | grep -i error; /tmp/c55_t | grep -v "^PASS" | grep -v "^$"; /tmp/c55_t | grep -c "^PASS" | sed 's/$/ lines PASS/'
echo; echo "== 2. C vs the independent Python node and simulator: the same seeds must give the same trace hash, with the correct node and with three deliberate bugs =="
gcc $F raft_cli.c $SRC -o /tmp/c55_cli; python3 diff_raft.py 400 /tmp/c55_cli
echo; echo "== 3. a long run of the simulator against the correct node: 20,000 seeds, every safety property checked after every tick =="
gcc $F raft_sweep.c $SRC -o /tmp/c55_sw && /tmp/c55_sw 1000 20000
