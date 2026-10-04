#!/usr/bin/env python3
"""Chapter 55: differential test. The C node and simulator (raft_cli, the SAME 055_raft.c and 055_sim.c the kernel links, with AddressSanitizer + UBSan) and the independent Python node and simulator (raft_ref.py) run the same seeds and must print the same line for each:
the fault profile, the first violation (if any), every statistic, and the trace hash of the whole run (every delivered message, every node's state after every tick). Seeds run with the correct node (no violation may appear) and with each of the six deliberate bugs, which must be found by both
at the same seed and tick. Usage: diff_raft.py N [cli]"""
import os, subprocess, sys
here = os.path.dirname(os.path.abspath(__file__)); ref = os.path.join(here, "..", "raft_ref.py")
N = int(sys.argv[1]); cli = sys.argv[2] if len(sys.argv) > 2 else "/tmp/raftcli"; diffs = 0; total = 0; viol_seen = {}
def run(prog, args): return subprocess.run(prog + [str(a) for a in args], capture_output=True, text=True).stdout.splitlines()
for name, args in (("the correct node", (0, N - 1)), ("the correct node, 600-tick runs", (N, N + N // 4 - 1, 0, 600)), ("bug 1 (a vote granted without checking the log)", (0, N // 2 - 1, 1)), ("bug 2 (an old-term entry committed by counting replicas)", (0, N // 2 - 1, 2)), ("bug 3 (a vote a restart forgets)", (10000, 10000 + N // 4, 3, 600)), ("bug 4 (no consistency check on AppendEntries)", (0, N // 2 - 1, 4)), ("bug 5 (a minority commits)", (0, N // 2 - 1, 5)), ("bug 6 (commitIndex may move backwards)", (0, N // 2 - 1, 6))):
    c, p = run([cli], args), run([sys.executable, ref], args); bad = 0
    for x, y in zip(c, p):
        if x != y:
            bad += 1
            if bad <= 2: print("DIFFERENT", name, "\n  C :", x[:200], "\n  Py:", y[:200])
    if len(c) != len(p): bad += 1
    v = sum(1 for l in c if l.split()[5] != "0"); total += len(c); diffs += bad; viol_seen[name] = (len(c), v, bad)
    print(f"  {name}: {len(c)} seeds, {v} with a violation found, {bad} differences")
print(f"{total} seeds compared, {diffs} differences"); sys.exit(1 if diffs else 0)
