#!/usr/bin/env python3
"""Chapter 13: test programs for the UNPACK instruction (and for int4 weights used by a matrix product). Same file format as ga2_progs.py. Usage: unpack_progs.py OUTFILE [NRANDOM] [SEED]
Each program loads packed words from external memory, expands them with UNPACK, and stores the result; some then multiply the expanded values (as B, int4 weights) by int8 activations and store that too."""
import os, random, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ga2_isa as I, ga2_progs as G
B = I.build; H = B("HALT")
def ext_image(R):
    img = [0] * I.EXT
    for i in range(0, 256): img[i] = R.getrandbits(32)                              # packed words: every nibble pattern
    img[0:6] = [0x76543210, 0xFEDCBA98, 0xFFFFFFFF, 0x00000000, 0x88888888, 0x77777777]    # extremes: all -1, all 0, all -8, all +7
    for i in range(256, 512): img[i] = R.randrange(-128, 128) & 0xFFFFFFFF            # int8 activations
    return img
def directed():
    P = []
    for n in (1, 2, 7, 33, 64):
        P.append([B("LD", dst=0, src=0, len=n), B("UNPACK", dst=200, src=0, len=n), B("ST", src=200, dst=1024, len=8 * n), H])
    P.append([B("LD", dst=0, src=0, len=100), B("UNPACK", dst=300, src=0, len=100), B("ST", src=300, dst=1100, len=800), H])       # 800 outputs: upper bits of the counters
    P.append([B("LD", dst=0, src=0, len=300), B("UNPACK", dst=400, src=0, len=300), B("ST", src=1800, dst=1024, len=1000), H])         # 300 packed words: the upper bits of the length field
    P.append([B("LD", dst=0, src=0, len=2), B("UNPACK", dst=50, src=0, len=2), B("LD", dst=100, src=256, len=16), B("MM", dst=400, A=100, B=50, M=1, K=4, N=4, tb=0, lda=4, ldb=4, ldc=4), B("ST", src=400, dst=1024, len=4), H])
    return P
def rand_program(R):
    prog = []; n = R.randrange(1, 40); prog += [B("LD", dst=0, src=R.randrange(0, 200), len=n), B("UNPACK", dst=100, src=0, len=n)]
    if R.random() < .6:                                                              # use the expanded values as a weight matrix
        K = R.randrange(1, 9); N = min(4, 8 * n // K) or 1
        if K * N <= 8 * n: prog += [B("LD", dst=60, src=256 + R.randrange(0, 200), len=K * 2), B("MM", dst=500, A=60, B=100, M=min(2, 1 + R.randrange(2)), K=K, N=N, tb=0, lda=K, ldb=N, ldc=N), B("ST", src=500, dst=1500, len=8)]
    prog += [B("ST", src=100, dst=1024, len=8 * n), H]
    return prog
def write(out, nrandom, seed):
    R = random.Random(seed); recs = [G.record(p, ext_image(R)) for p in directed()]
    for _ in range(nrandom): p = rand_program(R); recs.append(G.record(p, ext_image(R)))
    open(out, "w").write("\n".join("%08x" % (w & 0xFFFFFFFF) for w in [len(recs)] + [w for r in recs for w in r]) + "\n"); return len(recs)
if __name__ == "__main__":
    n = write(sys.argv[1], int(sys.argv[2]) if len(sys.argv) > 2 else 40, int(sys.argv[3]) if len(sys.argv) > 3 else 1); print(f"{n} programs written to {sys.argv[1]}")
