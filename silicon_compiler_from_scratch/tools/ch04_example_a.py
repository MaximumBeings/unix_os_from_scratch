#!/usr/bin/env python3
"""Chapter 4, running example A: watch the diamond. Run a 3 x 3 systolic array on a 3x4 times 4x3 product in Icarus, print the accumulators after every clock edge, and compare each frame with a closed-form prediction (the accumulator of PE(i,j) after edge t holds the sum of A[i][k]*B[k][j] over k <= t - i - j). Writes out/ch04_example_a.json for the figures. Usage: ch04_example_a.py"""
import json, os, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import hw
R = hw.ROOT; N, K = 3, 4
A = [[1, 2, 3, 4], [0, -1, 2, 1], [5, 1, -2, 0]]; B = [[1, 0, 2], [2, 1, 0], [0, -3, 1], [-1, 2, 1]]
C = [[sum(A[i][k] * B[k][j] for k in range(K)) for j in range(N)] for i in range(N)]
print("A =", A); print("B =", B); print("C = A x B =", C, "  (ordinary matrix multiplication, in Python)\n")
subprocess.run(["iverilog", "-g2012", "-s", "systolic_trace_tb", "-o", "/tmp/sys_tr.vvp", "rtl/systolic.v", "tb/systolic_trace_tb.v"], cwd=R, capture_output=True, check=True)
out = subprocess.run(["vvp", "-n", "/tmp/sys_tr.vvp"], cwd=R, capture_output=True, text=True).stdout
frames = [list(map(int, l.split()[1:])) for l in out.splitlines() if l.startswith("F ")]
ok = True; log = []
print("accumulators after each clock edge t (circuit)   [PE(0,0) PE(0,1) PE(0,2) / PE(1,0) ... / PE(2,2)]    prediction agrees?")
for fr in frames:
    t = fr[0]; acc = fr[1:]; pred = [sum(A[i][k] * B[k][j] for k in range(K) if k <= t - i - j) for i in range(N) for j in range(N)]; good = acc == pred; ok &= good; log.append({"t": t, "acc": acc, "busy": [(i, j) for i in range(N) for j in range(N) if 0 <= t - i - j < K]})
    rows = "  ".join("[" + " ".join(f"{acc[3*i+j]:4d}" for j in range(N)) + "]" for i in range(N))
    print(f"  t={t}: {rows}   {'yes' if good else 'NO'}")
final = frames[-1][1:]; print("\nfinal accumulators equal C:", final == [v for r in C for v in r]); ok &= final == [v for r in C for v in r]
done = {}
for fr in frames:
    for i in range(N):
        for j in range(N):
            if (i, j) not in done and fr[1 + 3 * i + j] == C[i][j] and fr[0] >= i + j + K - 1: done[(i, j)] = fr[0]
print("cycle at which each PE finishes (its last product is at t = i + j + K - 1 = i + j + 3):")
for i in range(N): print("  ", [done[(i, j)] for j in range(N)])
print("PE(0,0) is done at t=3, PE(2,2) at t=7: the product takes K + 2N - 2 = 8 edges (t = 0..7). The two extra frames (t = 8, 9) show the values holding, because zeros keep being fed.")
json.dump({"A": A, "B": B, "C": C, "frames": log}, open(os.path.join(R, "out", "ch04_example_a.json"), "w"))
sys.exit(0 if ok else 1)
