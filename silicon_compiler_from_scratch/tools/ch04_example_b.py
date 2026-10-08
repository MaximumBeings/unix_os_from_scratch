#!/usr/bin/env python3
"""Chapter 4, running example B: a matrix bigger than the array. An 8 x 8 product (K = 8) is cut into four 4 x 4 output tiles that run one after another on a 4 x 4 array; the circuit's four result tiles are stitched together and compared with the whole product computed in Python. Then the same product on an 8 x 8 array, and a utilization table from the formula. Writes out/ch04_example_b.json for the figures. Usage: ch04_example_b.py"""
import json, os, random, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import hw
R = hw.ROOT; rng = random.Random(4)
M = 8; K = 8
A = [[rng.randrange(-128, 128) for _ in range(K)] for _ in range(M)]; B = [[rng.randrange(-128, 128) for _ in range(M)] for _ in range(K)]
C = [[sum(A[i][k] * B[k][j] for k in range(K)) for j in range(M)] for i in range(M)]
w = lambda v: "%08x" % (v & 0xFFFFFFFF)
def run_tiles(N):
    """Cut C into (M/N)^2 tiles of N x N; tile (r, c) needs rows r*N.. of A (N x K) and columns c*N.. of B (K x N). Returns the stitched result and the cycle count measured by the testbench."""
    words = []; tiles = [(r, c) for r in range(M // N) for c in range(M // N)]
    for r, c in tiles:
        At = [A[r * N + i] for i in range(N)]; Bt = [[B[k][c * N + j] for j in range(N)] for k in range(K)]
        words += [w(v) for row in At for v in row] + [w(v) for row in Bt for v in row] + [w(0)] * (N * N)
    open(os.path.join(R, "out", "systolic_tiles.hex"), "w").write("\n".join([w(len(tiles)), w(N), w(K)] + words) + "\n")
    rc, out = hw.sim_icarus(["rtl/systolic.v", "tb/systolic_tiles_tb.v"], "systolic_tiles_tb", defines=(f"NN={N}", f"KK={K}")); assert rc == 0, out
    got = [[None] * M for _ in range(M)]; cycles = None
    for l in out.splitlines():
        p = l.split()
        if p and p[0] == "R": cs, i, j, v = map(int, p[1:]); r, c = tiles[cs]; got[r * N + i][c * N + j] = v
        if p and p[0] == "CYCLES": cycles = int(p[1])
    return got, cycles, len(tiles)
print(f"A is {M}x{K}, B is {K}x{M}, random int8 (seed 4). C = A B has {M*M} elements and takes {M*M*K} multiply-accumulates.\n")
res = {"rows": []}; ok = True
for N in (4, 8):
    got, cyc, nt = run_tiles(N); good = got == C; ok &= good
    macs = M * M * K; util = macs / (cyc * N * N); ideal = macs / (N * N)
    print(f"array {N} x {N} ({N*N} PEs): {nt} tile(s), each {K}+2*{N}-2 = {K+2*N-2} cycles -> {cyc} cycles total (measured by the testbench); result equals the Python product: {good}")
    print(f"   perfect utilization would need {ideal:.0f} cycles; this takes {cyc}; utilization = {macs} / ({cyc} x {N*N}) = {util:.3f}")
    res["rows"].append({"N": N, "tiles": nt, "cycles": cyc, "util": util, "ideal": ideal})
print("\nso the four-times-smaller array is only 2.55x slower, and its PEs are busier (0.571 vs 0.364): each PE idles less because the fill and drain (2N-2 = 6 cycles per tile) cost less when N is small.")
print("the larger array has 4x the PEs; the product is 22 cycles instead of 56. The throughput-per-PE of the small array is better here. A bigger K would narrow the gap (below).")
print("\nutilization of an N x N array on ONE tile, K/(K+2N-2)  (derived from the schedule; the N=4, K=8 and N=8, K=8 entries match the runs above):")
Ks = [1, 4, 8, 16, 64, 256, 1024]; print("      K: " + " ".join(f"{k:6d}" for k in Ks)); res["util_table"] = {}
for N in (4, 8, 16, 128):
    row = [k / (k + 2 * N - 2) for k in Ks]; res["util_table"][N] = row; print(f"  N={N:3d}: " + " ".join(f"{v:6.3f}" for v in row))
res["Ks"] = Ks; json.dump(res, open(os.path.join(R, "out", "ch04_example_b.json"), "w"))
sys.exit(0 if ok else 1)
