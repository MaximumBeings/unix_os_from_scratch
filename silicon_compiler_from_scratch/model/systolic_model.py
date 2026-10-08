#!/usr/bin/env python3
"""Chapter 4: the answer key for the systolic array, plus a cycle-by-cycle picture of the wavefront.
The answer is ordinary matrix multiplication, C[i][j] = sum_k A[i][k] * B[k][j] in exact integers (the accumulators wrap at 32 bits, which these sizes never reach, and the model checks that).
The vector file is a stream of 32-bit hex words: [ncases, N, K] then, per case, A (N x K, row-major), B (K x N, row-major) and C (N x N). int8 values are sign-extended to 32 bits.
Usage: systolic_model.py OUTFILE N K [NRANDOM]      or      systolic_model.py --wave N K   (prints which A[i][k] * B[k][j] each PE performs in each cycle)"""
import random, sys
def matmul(A, B): return [[sum(A[i][k] * B[k][j] for k in range(len(B))) for j in range(len(B[0]))] for i in range(len(A))]
def cases(N, K, nrandom, seed):
    R = random.Random(seed); out = []
    rnd = lambda: [[R.randrange(-128, 128) for _ in range(K)] for _ in range(N)]
    rndb = lambda: [[R.randrange(-128, 128) for _ in range(N)] for _ in range(K)]
    out.append(([[0] * K for _ in range(N)], rndb()))                                    # A = 0
    out.append(([[-128] * K for _ in range(N)], [[-128] * N for _ in range(K)]))         # the largest possible sums
    out.append(([[-128] * K for _ in range(N)], [[127] * N for _ in range(K)]))          # the most negative
    out.append(([[1 if i == k else 0 for k in range(K)] for i in range(N)], [[R.randrange(-128, 128) for _ in range(N)] for _ in range(K)]))   # selects rows of B
    out.append(([[(i * K + k) % 7 - 3 for k in range(K)] for i in range(N)], [[(k * N + j) % 5 - 2 for j in range(N)] for k in range(K)]))     # small patterned values: transposition shows
    for _ in range(nrandom): out.append((rnd(), rndb()))
    return out
def wave(N, K):
    cycles = K + 2 * N - 2; macs = 0
    for t in range(cycles):
        busy = [(i, j, t - i - j) for i in range(N) for j in range(N) if 0 <= t - i - j < K]; macs += len(busy)
        print(f"cycle {t:2d}: " + (" ".join(f"PE({i},{j})<-A[{i}][{k}]*B[{k}][{j}]" for i, j, k in busy) if len(busy) <= 4 else f"{len(busy)} of {N*N} PEs busy"))
    print(f"{macs} multiply-accumulates in {cycles} cycles on {N*N} PEs: utilization {macs / (cycles * N * N):.3f} = K/(K+2N-2) = {K / cycles:.3f}")
if __name__ == "__main__":
    if sys.argv[1] == "--wave": wave(int(sys.argv[2]), int(sys.argv[3])); sys.exit()
    out, N, K = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]); nr = int(sys.argv[4]) if len(sys.argv) > 4 else 200
    cs = cases(N, K, nr, 1000 * N + K); w = lambda v: "%08x" % (v & 0xFFFFFFFF); words = [len(cs), N, K]
    for A, B in cs:
        C = matmul(A, B); assert all(-2**31 <= v < 2**31 for r in C for v in r)
        words += [w(v) if isinstance(v, int) else v for v in [x for r in A for x in r] + [x for r in B for x in r] + [x for r in C for x in r]]
    open(out, "w").write("\n".join(w(x) if isinstance(x, int) else x for x in words) + "\n"); print(f"{len(cs)} matrix products (N={N}, K={K}) written to {out}")
