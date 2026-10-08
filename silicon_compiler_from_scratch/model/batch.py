#!/usr/bin/env python3
"""Chapter 9: decode steps for several sequences at once ("batching"), as GA-2 programs.
The B sequences share one set of weights but each has its own hidden states and its own K and V caches, possibly at different lengths (as in a server where requests arrive at different times).
What batching shares: the weights. The projections of the B new tokens are ONE matrix product with M = B rows instead of B products with M = 1, so the weights are loaded once and the 4 x 4 array has B rows to work on.
What it cannot share: attention. Each sequence attends over its own cache, so scores and weighted sums stay per sequence.
Memory plan (words).  EXTERNAL (sequence b, 0..3): X_b at 2048 + 256 b, K cache 3072 + 256 b, V cache 4096 + 256 b, output 5120 + 16 b; the shared weights at 512, 768, 1024.  Contexts of up to 16 tokens.
SCRATCHPAD: see SP_* (batch slot j holds the j-th sequence of the batch)."""
import math, os, random, sys
os.environ.setdefault("GA2_EXT", "8192")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ga2_isa as I
from quant import quantize, scale_for, mantissa_shift, requant, matmul_int8
from softmax_gold import softmax_fixed
DM, D, LMAX = 16, 16, 16
EX_WQ = 512
def EX_X(b): return 2048 + 256 * b
def EX_KC(b): return 3072 + 256 * b
def EX_VC(b): return 4096 + 256 * b
def EX_OUT(b): return 5120 + 16 * b
SP_X, SP_W, SP_QA, SP_KA, SP_VA, SP_Q8 = 0, 64, 832, 896, 960, 1024
SP_S, SP_S8, SP_P, SP_WT, SP_O, SP_O8 = 1088, 1120, 1152, 1184, 1216, 1232
def SP_KC(j): return 1280 + 512 * j
def SP_VC(j): return 1280 + 512 * j + 256
B_ = I.build
def make_batch(nseq, L, seed):
    """nseq sequences of L tokens with shared weights; ONE set of scales for the whole batch (a model has one set of scales, not one per request)."""
    R = random.Random(seed); g = lambda r, c, sd: [[R.gauss(0, sd) for _ in range(c)] for _ in range(r)]
    W = [g(DM, D, 1 / math.sqrt(DM)) for _ in range(3)]; Xs = [g(L, DM, 1.0) for _ in range(nseq)]
    mm = lambda A, Bm: [[sum(A[i][t] * Bm[t][j] for t in range(len(Bm))) for j in range(len(Bm[0]))] for i in range(len(A))]
    flat = lambda Ms: [v for M in Ms for r in M for v in r]
    Qs, Ks, Vs = ([mm(X, w) for X in Xs] for w in W)
    outs = []
    for b in range(nseq):
        for t in range(1, L + 1):
            q = Qs[b][t - 1]; sc = [sum(q[i] * Ks[b][p][i] for i in range(D)) / math.sqrt(D) for p in range(t)]; mx = max(sc); e = [math.exp(v - mx) for v in sc]; z = sum(e)
            outs.append([sum(e[p] / z * Vs[b][p][i] for p in range(t)) for i in range(D)])
    sc_ = {"x": scale_for(flat(Xs)), "wq": scale_for(flat([W[0]])), "wk": scale_for(flat([W[1]])), "wv": scale_for(flat([W[2]])), "q": scale_for(flat(Qs)), "k": scale_for(flat(Ks)), "v": scale_for(flat(Vs)), "o": scale_for([v for o in outs for v in o])}
    return {"nseq": nseq, "L": L, "Xs": Xs, "W": W, "scales": sc_}
def rq_params(ex):
    s = ex["scales"]; ms = mantissa_shift
    return {"q": ms(s["x"] * s["wq"] / s["q"]), "k": ms(s["x"] * s["wk"] / s["k"]), "v": ms(s["x"] * s["wv"] / s["v"]), "scores": ms(s["q"] * s["k"] / math.sqrt(D) * 16), "weights": ms(127 / 65536), "out": ms(s["v"] / (127 * s["o"]))}
def ext_image(ex):
    s = ex["scales"]; img = [0] * I.EXT; w32 = lambda v: v & 0xFFFFFFFF
    for wi, nm in enumerate(("wq", "wk", "wv")):
        for r in range(DM):
            for c in range(D): img[EX_WQ + 256 * wi + r * D + c] = w32(quantize(ex["W"][wi][r][c], s[nm]))
    for b in range(ex["nseq"]):
        for t in range(ex["L"]):
            for c in range(DM): img[EX_X(b) + t * DM + c] = w32(quantize(ex["Xs"][b][t][c], s["x"]))
    return img
def program(seqs, ts, P):
    """One decode step for the sequences `seqs` (indices into the external layout), which have `ts[i]` tokens each including the new one. Batch slot i holds sequence seqs[i]."""
    n = len(seqs); prog = []
    for j, b in enumerate(seqs): prog.append(B_("LD", dst=SP_X + j * DM, src=EX_X(b) + (ts[j] - 1) * DM, len=DM))
    prog.append(B_("LD", dst=SP_W, src=EX_WQ, len=3 * DM * D))
    for j, b in enumerate(seqs):
        if ts[j] > 1: prog += [B_("LD", dst=SP_KC(j), src=EX_KC(b), len=(ts[j] - 1) * D), B_("LD", dst=SP_VC(j), src=EX_VC(b), len=(ts[j] - 1) * D)]
    for wi, acc in enumerate((SP_QA, SP_KA, SP_VA)):                                    # the projections: ONE matrix product with M = n rows per column block
        for c in range(D // 4): prog.append(B_("MM", dst=acc + 4 * c, A=SP_X, B=SP_W + 256 * wi + 4 * c, M=n, K=DM, N=4, tb=0, lda=DM, ldb=D, ldc=D))
    prog.append(B_("RQ", dst=SP_Q8, src=SP_QA, len=n * D, m=P["q"][0], s=P["q"][1]))
    for j, b in enumerate(seqs):
        r = (ts[j] - 1) * D
        prog += [B_("RQ", dst=SP_KC(j) + r, src=SP_KA + j * D, len=D, m=P["k"][0], s=P["k"][1]), B_("RQ", dst=SP_VC(j) + r, src=SP_VA + j * D, len=D, m=P["v"][0], s=P["v"][1]),
                 B_("ST", src=SP_KC(j) + r, dst=EX_KC(b) + r, len=D), B_("ST", src=SP_VC(j) + r, dst=EX_VC(b) + r, len=D)]
    for j, b in enumerate(seqs):                                                         # attention, one sequence at a time
        L = ts[j]
        for c in range((L + 3) // 4): prog.append(B_("MM", dst=SP_S + 4 * c, A=SP_Q8 + j * D, B=SP_KC(j) + 4 * c * D, M=1, K=D, N=min(4, L - 4 * c), tb=1, lda=D, ldb=D, ldc=4))
        prog += [B_("RQ", dst=SP_S8, src=SP_S, len=L, m=P["scores"][0], s=P["scores"][1]), B_("SM", dst=SP_P, src=SP_S8, len=L), B_("RQ", dst=SP_WT, src=SP_P, len=L, m=P["weights"][0], s=P["weights"][1])]
        for c in range(D // 4): prog.append(B_("MM", dst=SP_O + 4 * c, A=SP_WT, B=SP_VC(j) + 4 * c, M=1, K=L, N=4, tb=0, lda=L, ldb=D, ldc=4))
        prog += [B_("RQ", dst=SP_O8, src=SP_O, len=D, m=P["out"][0], s=P["out"][1]), B_("ST", src=SP_O8, dst=EX_OUT(b), len=D)]
    return prog + [B_("HALT")]
def outputs(ext, seqs): return {b: [I.s8(ext[EX_OUT(b) + i]) for i in range(D)] for b in seqs}
def int_reference(ex, b, t):
    """Plain-Python integer result of sequence b at its t-th token (no instructions, no addresses)."""
    s = ex["scales"]; P = rq_params(ex)
    Xq = [[quantize(v, s["x"]) for v in r] for r in ex["Xs"][b][:t]]; Wq = [[[quantize(v, s[nm]) for v in r] for r in W] for W, nm in zip(ex["W"], ("wq", "wk", "wv"))]
    rqm = lambda M, key: [[requant(a, *P[key]) for a in r] for r in M]
    q8 = rqm(matmul_int8([Xq[-1]], Wq[0]), "q")[0]; K8 = rqm(matmul_int8(Xq, Wq[1]), "k"); V8 = rqm(matmul_int8(Xq, Wq[2]), "v")
    sc = [requant(sum(q8[i] * K8[p][i] for i in range(D)), *P["scores"]) for p in range(t)]; w = [requant(v, *P["weights"]) for v in softmax_fixed(sc)]
    return [requant(sum(w[p] * V8[p][c] for p in range(t)), *P["out"]) for c in range(D)]
def run(ext, prog): return I.Machine(ext).run(prog)
def traffic(prog):
    """(MACs, words moved to or from external memory) of a program. One word is one int8 element in this design's logical accounting."""
    macs = sum(i["f"] * i["g"] * i["h"] for i in prog if i["op"] == "MM"); words = sum(i["c"] for i in prog if i["op"] in ("LD", "ST")); return macs, words
def check_batching(nseq_list=(1, 2, 3, 4), lengths=(1, 2, 5, 9, 16), seeds=range(2), exact=True, ragged=True):
    """Batch invariance: at every step, each sequence's output and cache rows must equal what the same sequence produces when decoded ALONE (batch of one). Also ragged batches (different lengths).
    And each sequence decoded alone must equal the integer reference exactly (batch invariance alone is blind to a bug that the batch and the single run share). Returns the list of problems."""
    problems = []
    for nseq in nseq_list:
        for L in lengths:
            for seed in seeds:
                ex = make_batch(nseq, L, 7000 + 31 * nseq + 5 * L + seed); P = rq_params(ex)
                ext_b = ext_image(ex); ext_s = [list(ext_image(ex)) for _ in range(nseq)]; seqs = list(range(nseq))
                for t in range(1, L + 1):
                    m = run(ext_b, program(seqs, [t] * nseq, P)); ext_b = m.ext
                    for b in seqs:
                        ext_s[b] = run(ext_s[b], program([b], [t], P)).ext
                        if outputs(ext_b, [b])[b] != outputs(ext_s[b], [b])[b]: problems.append(f"B={nseq} L={L} seed={seed} step {t}: sequence {b} output differs from decoding it alone")
                        if ext_b[EX_KC(b):EX_KC(b) + 256] != ext_s[b][EX_KC(b):EX_KC(b) + 256] or ext_b[EX_VC(b):EX_VC(b) + 256] != ext_s[b][EX_VC(b):EX_VC(b) + 256]: problems.append(f"B={nseq} L={L} seed={seed} step {t}: sequence {b} cache differs")
                        if exact and outputs(ext_s[b], [b])[b] != int_reference(ex, b, t): problems.append(f"B={nseq} L={L} seed={seed} step {t}: sequence {b} decoded alone differs from the integer reference")
    # ragged: sequences at different positions share one step
    for seed in (seeds if ragged else []):
        ex = make_batch(4, 16, 9100 + seed); P = rq_params(ex); lens = [3, 9, 1, 14]; ext_b = ext_image(ex); seqs = [0, 1, 2, 3]
        for b in seqs:
            for t in range(1, lens[b]): ext_b = run(ext_b, program([b], [t], P)).ext
        ext_s = [list(ext_b) for _ in seqs]; ext_b = run(ext_b, program(seqs, lens, P)).ext
        for b in seqs:
            ext_s[b] = run(ext_s[b], program([b], [lens[b]], P)).ext
            if outputs(ext_b, [b])[b] != outputs(ext_s[b], [b])[b]: problems.append(f"ragged seed={seed}: sequence {b} (length {lens[b]}) differs from decoding it alone")
            if exact and outputs(ext_s[b], [b])[b] != int_reference(ex, b, lens[b]): problems.append(f"ragged seed={seed}: sequence {b} differs from the integer reference")
    return problems
if __name__ == "__main__":
    print("problems:", check_batching())
