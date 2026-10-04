#!/usr/bin/env python3
"""Chapter 55: verification FROM OUTSIDE THE KERNEL. It reads the serial capture and shares no code with the kernel: raft_ref.py is a second implementation of the Raft node AND of the deterministic simulator, written from the paper and from the simulator's specification.
  1. the twelve seed lines the kernel printed (profile, violations, every statistic, and the TRACE HASH of the whole run: a hash of every message delivered and of every node's state after every tick) must equal the Python simulator's, character for character;
  2. the final state of every node of two runs must equal the Python nodes' (role, term, log length, commit, commands applied, state machine hash);
  3. the sweep of 120 seeds: Python re-runs all 120 and re-adds the per-profile totals;
  4. the two deliberate bugs: Python runs the same seed search and must find the same seed, tick, node and trace hash, and the replay in the kernel must be identical;
  5. the kernel's tally.
Usage: verify_055.py SERIAL_TXT"""
import os, re, sys
here = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, here)
import raft_ref as R
fails = 0; serial = open(sys.argv[1], errors="replace").read().replace("\r", "")
def report(ok, msg):
    global fails; fails += not ok; print(("  ok   " if ok else "  FAIL ") + msg)
def block(name): return re.search(rf"@@RAFT {name} BEGIN\n(.*?)@@RAFT {name} END", serial, re.S).group(1)
print("-- 1. twelve seeds, compared line for line with the Python simulator")
lines = block("SEEDS").splitlines(); ok = True
for seed in range(12):
    c, s = R.run(seed); want = R.line(seed, c, s)
    if lines[seed] != want: ok = False; print("    kernel:", lines[seed]); print("    python:", want)
report(ok and len(lines) == 12, "all 12 lines (profile, statistics, trace hash of every message and every tick) are identical")
print("-- 2. final node states")
RN = ("follower", "candidate", "leader")
for seed in (4, 43):
    c, s = R.run(seed); m = re.search(rf"final state of the five nodes of seed {seed} .*?\n((?:    node .*\n){{5}})", serial); got = m.group(1).strip().splitlines() if m else []
    want = [f"node {i}: {'up  ' if s.alive[i] else 'DOWN'}, {RN[s.node[i].role]}, term {s.node[i].term}, log length {len(s.node[i].log)}, commit {s.node[i].commit}, applied {s.node[i].applied}, state machine: {s.node[i].sm_count} commands, hash {s.node[i].sm_hash:x}" for i in range(5)]
    report([g.strip() for g in got] == want, f"seed {seed}: the five nodes' role, term, log length, commit index, applied count and state machine hash equal the Python nodes'")
print("-- 3. the sweep of 120 seeds, re-run in Python")
tot = {p: dict(el=0, pr=0, ak=0, cr=0, ms=0, bad=0) for p in range(4)}
for seed in range(100, 220):
    c, s = R.run(seed); t = tot[seed % 4]; t["el"] += s.elections; t["pr"] += s.proposals; t["ak"] += s.acked; t["cr"] += s.crashes; t["ms"] += s.delivered; t["bad"] += 1 if s.viol else 0
names = ("calm network", "loss and reordering", "partitions", "everything + leader crashes"); sw = block("SWEEP").splitlines(); ok = True
for p in range(4):
    t = tot[p]; want = f"profile {p} ({names[p]}): elections {t['el']}, client commands proposed {t['pr']}, acknowledged {t['ak']}, crashes {t['cr']}, messages delivered {t['ms']}, violations {t['bad']}"
    if sw[p] != want: ok = False; print("    kernel:", sw[p]); print("    python:", want)
report(ok, "the four per-profile lines (elections, commands proposed and acknowledged, crashes, messages delivered, violations) are identical; total violations by the correct node: " + str(sum(t["bad"] for t in tot.values())))
print("-- 4. the two deliberate bugs")
bl = block("BUGS").splitlines()
for which, (bug, frm, ticks) in enumerate(((1, 0, 300), (3, 10000, 600))):
    R.bug = bug; found = None
    for seed in range(frm, frm + 400):
        c, s = R.run(seed, ticks)
        if s.viol: found = (seed, c, s); break
    R.bug = 0
    seed, c, s = found; want = f"bug {bug} seed {seed} ticks {ticks} violation {s.viol} tick {s.viol_tick} node {s.viol_node} trace {s.trace:08x} replay_identical 1"
    report(bl[2 * which] == want, f"bug {bug}: the Python search finds the same seed ({seed}), violation ({s.viol}), tick ({s.viol_tick}), node ({s.viol_node}) and trace hash ({s.trace:08x}); the kernel's replay of the seed was identical")
t = re.search(r"Raft demo complete: (\d+) seeds simulated, (\d+) violations by the correct node, (\d+) deliberate bugs found, (\d+) replays identical", serial)
report(t and tuple(map(int, t.groups())) == (132, 0, 2, 2), "final tally: 132 seeds simulated, 0 violations by the correct node, 2 deliberate bugs found, 2 replays identical")
print(f"\n{'ALL CHECKS PASSED' if not fails else str(fails) + ' CHECK(S) FAILED'}"); sys.exit(1 if fails else 0)
