#!/usr/bin/env python3
"""GA-2 test programs and the file the testbench reads. Usage: ga2_progs.py OUTFILE [NRANDOM] [SEED]
The file is a stream of 32-bit hex words: [nprograms] then, per program: [ninstructions], 4 words per instruction (most significant first), EXT words of initial external memory,
EXT words of the expected external memory after the program, and 5 expected counters: total cycles, MM cycles, DMA cycles, vector-unit cycles, instructions.
Memory plan for the random programs: ext[0..511] int8 data, ext[512..1023] arbitrary 32-bit data, ext[1024..2047] results. spad[0..2047] loaded inputs (arenas), spad[2048..3071] results."""
import os, random, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ga2_isa as I
EXT = I.EXT
def ext_image(R):
    img = [0] * EXT
    for i in range(512): img[i] = R.randrange(-128, 128) & 0xFFFFFFFF
    for i in range(512, 1024): img[i] = R.getrandbits(32)
    return img
def directed():
    """One small program per behaviour worth naming; each loads data, does one thing and stores the result."""
    P = []
    ld = lambda dst, src, n: I.build("LD", dst=dst, src=src, len=n)
    stt = lambda src, dst, n: I.build("ST", src=src, dst=dst, len=n)
    H = I.build("HALT")
    P.append([ld(0, 0, 16), stt(0, 1024, 16), H])                                                          # copy through
    P.append([ld(0, 0, 1), stt(0, 1024, 1), H])                                                            # length one
    P.append([ld(0, 0, 64), I.build("MM", dst=2048, A=0, B=16, M=1, K=1, N=1), stt(2048, 1024, 1), H])    # 1x1x1 matrix product
    P.append([ld(0, 0, 256), I.build("MM", dst=2048, A=0, B=128, M=4, K=64, N=4, tb=0, lda=64, ldb=4, ldc=4), stt(2048, 1024, 16), H])   # largest tile
    P.append([ld(0, 0, 256), I.build("MM", dst=2048, A=0, B=128, M=3, K=7, N=2, tb=1, lda=7, ldb=7, ldc=2), stt(2048, 1024, 6), H])        # transposed B
    P.append([ld(0, 512, 32), I.build("RQ", dst=2048, src=0, len=32, m=(1 << 23) + 12345, s=30, relu=0), stt(2048, 1024, 32), H])
    P.append([ld(0, 512, 32), I.build("RQ", dst=2048, src=0, len=32, m=(1 << 24) - 1, s=24, relu=1), stt(2048, 1024, 32), H])
    P.append([ld(0, 0, 16), I.build("SM", dst=2048, src=0, len=16), stt(2048, 1024, 16), H])
    P.append([ld(0, 0, 1), I.build("SM", dst=2048, src=0, len=1), stt(2048, 1024, 1), H])
    P.append([ld(0, 0, 64), ld(64, 0, 64), I.build("VADD", dst=2048, src1=0, src2=64, len=64), stt(2048, 1024, 64), H])
    P.append([ld(0, 512, 40), I.build("AMAX", dst=2048, src=0, len=40), stt(2048, 1024, 1), H])
    # long vectors: lengths above 255 exercise the upper bits of every length field
    P.append([ld(0, 0, 1500), stt(0, 548, 1500), H])
    P.append([ld(0, 0, 1000), ld(1000, 0, 1000), I.build("VADD", dst=2048, src1=0, src2=1000, len=1000), stt(2048, 1024, 1000), H])
    P.append([ld(0, 512, 500), I.build("RQ", dst=2048, src=0, len=500, m=(1 << 23) + 777, s=29, relu=0), stt(2048, 1024, 500), H])
    P.append([ld(0, 0, 600), I.build("SM", dst=2048, src=0, len=600), stt(2048, 1024, 600), H])
    P.append([ld(0, 0, 400), ld(400, 0, 400), I.build("AMAX", dst=2048, src=0, len=800), stt(2048, 1024, 1), H])
    P.append([ld(0, 0, 8), I.build("AMAX", dst=2048, src=0, len=8), stt(2048, 1024, 1), H])
    P.append([H])                                                                                          # halt at once
    return P
def rand_program(R):
    prog = []; ld = lambda dst, src, n: prog.append(I.build("LD", dst=dst, src=src, len=n))
    for a in range(4):                                                                                      # fill the arenas
        ld(a * 512, R.choice([0, 0, 256, 512, 768]) if R.random() < 0.8 else R.randrange(0, 960), R.randrange(64, 256))
    out = [2048]                                                                                             # next free result address
    def take(n): a = out[0]; out[0] = min(a + n + R.randrange(0, 8), 3072 - 1); return min(a, 3072 - n)
    def src_region(n): return R.randrange(0, 2048 - n) if R.random() < 0.7 else R.randrange(2048, 3072 - n)
    def ok(d, n, srcs): return all(d == s0 or d + n <= s0 or s0 + n <= d for s0 in srcs)       # regions must be identical or disjoint (RQ, SM, VADD are pipelined)
    def pick(n, srcs):
        inpl = [s0 for s0 in srcs if s0 >= 2048 and ok(s0, n, srcs)]
        if inpl and R.random() < 0.3: return inpl[0]
        for _ in range(60):
            d = take(n) if R.random() < 0.5 else R.randrange(2048, 3072 - n)
            if ok(d, n, srcs): return d
        return None
    for _ in range(R.randrange(10, 26)):
        kind = R.choice(["MM", "MM", "MM", "RQ", "RQ", "SM", "SM", "VADD", "AMAX", "LD", "ST"])
        if kind == "MM":
            M, N, K = R.randrange(1, 5), R.randrange(1, 5), R.choice([R.randrange(1, 65), R.randrange(1, 9), 64, 1]); tb = R.randrange(2)
            lda = K + R.randrange(0, 4); ldb = (K if tb else N) + R.randrange(0, 4); ldc = N + R.randrange(0, 3)
            aext = (M - 1) * lda + K; bext = ((N - 1) * ldb + K) if tb else ((K - 1) * ldb + N); cext = (M - 1) * ldc + N
            prog.append(I.build("MM", dst=take(cext), A=src_region(aext), B=src_region(bext), M=M, K=K, N=N, tb=tb, lda=lda, ldb=ldb, ldc=ldc))
        elif kind == "RQ":
            n = R.randrange(1, 65); s = src_region(n); d = pick(n, [s])
            if d is None: continue
            prog.append(I.build("RQ", dst=d, src=s, len=n, m=R.randrange(1 << 23, 1 << 24), s=R.randrange(0, 64) if R.random() < 0.3 else R.randrange(14, 40), relu=R.randrange(2)))
        elif kind == "SM":
            n = R.choice([R.randrange(1, 65), R.randrange(1, 9), 64]); s = src_region(n); d = pick(n, [s])
            if d is None: continue
            prog.append(I.build("SM", dst=d, src=s, len=n))
        elif kind == "VADD":
            n = R.randrange(1, 65); s1 = src_region(n); s2 = src_region(n); d = pick(n, [s1, s2])
            if d is None: continue
            prog.append(I.build("VADD", dst=d, src1=s1, src2=s2, len=n))
        elif kind == "AMAX":
            n = R.randrange(1, 65); prog.append(I.build("AMAX", dst=take(1), src=src_region(n), len=n))
        elif kind == "LD":
            n = R.randrange(1, 100); prog.append(I.build("LD", dst=R.randrange(0, 2048 - n), src=R.randrange(0, 1024 - n), len=n))
        else:
            n = R.randrange(1, 65); prog.append(I.build("ST", src=src_region(n), dst=R.randrange(1024, 2048 - n), len=n))
    prog.append(I.build("ST", src=2048, dst=1024, len=1024)); prog.append(I.build("HALT"))                  # results out
    return prog
def record(prog, ext0):
    m = I.Machine(ext0).run(prog); words = [len(prog)]
    for ins in prog:
        w = I.encode(ins); words += [(w >> 96) & 0xFFFFFFFF, (w >> 64) & 0xFFFFFFFF, (w >> 32) & 0xFFFFFFFF, w & 0xFFFFFFFF]
    return words + list(ext0) + m.ext + I.cycle_counts(prog)
def write(out, nrandom, seed):
    R = random.Random(seed); recs = []
    for p in directed(): recs.append(record(p, ext_image(R)))
    for _ in range(nrandom): recs.append(record(rand_program(R), ext_image(R)))
    open(out, "w").write("\n".join("%08x" % (w & 0xFFFFFFFF) for w in [len(recs)] + [w for r in recs for w in r]) + "\n"); return len(recs)
if __name__ == "__main__":
    n = write(sys.argv[1], int(sys.argv[2]) if len(sys.argv) > 2 else 60, int(sys.argv[3]) if len(sys.argv) > 3 else 1)
    print(f"{n} programs written to {sys.argv[1]} ({len(directed())} directed, {n - len(directed())} random)")
