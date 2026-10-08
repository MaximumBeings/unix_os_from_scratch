#!/usr/bin/env python3
"""Chapter 8: one attention head, for one decode step, as GA-2 programs, with a floating-point reference.
THE MATHS.  x_t is the hidden state of the newest token (Dm numbers).  q = x_t Wq,  k_t = x_t Wk,  v_t = x_t Wv.  The keys K and values V of all L tokens so far (the last row is k_t, v_t) are the KV CACHE.
  scores = K q / sqrt(d)      weights = softmax(scores)      out = weights V
Every tensor is int8 with one scale each; the int32 results of a matrix product are brought back to int8 by a requantizer with M = (scale_a * scale_b) / scale_out (Chapter 3).
TWO PROGRAMS for the same step:
  'cached'    the caches of the L-1 earlier tokens live in external memory; compute q, k_t, v_t for the new token only, append k_t and v_t to the caches, attend.
  'recompute' no cache: recompute K and V for ALL L tokens from their hidden states every step, then attend.   (They produce identical outputs; the cost is the difference.)
Memory plan (words).  EXTERNAL: X at 0 (L x Dm), Wq 512, Wk 768, Wv 1024 (Dm x d each), K cache 2048, V cache 2560, output 3072.  SCRATCHPAD: see SP_* below."""
import math, os, random, sys
os.environ.setdefault("GA2_EXT", "8192")                     # the attention programs need an 8192-word external memory (set before ga2_isa is imported)
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ga2_isa as I
from quant import quantize, scale_for, mantissa_shift, requant, matmul_int8
from softmax_gold import softmax_fixed
DM, D = 16, 16                                                     # model width and head width
EX_X, EX_WQ, EX_WK, EX_WV, EX_KC, EX_VC, EX_OUT = 0, 512, 768, 1024, 2048, 2560, 3072
SP_X, SP_WQ, SP_WK, SP_WV, SP_ACC, SP_KC, SP_VC = 0, 512, 768, 1024, 1280, 1792, 2304
SP_Q, SP_Q8, SP_S, SP_S8, SP_P, SP_W, SP_O, SP_O8, SP_KN, SP_VN = 2816, 2832, 2848, 2880, 2912, 2944, 2976, 2992, 3008, 3024
B = I.build
# ---------------------------------------------------------------- data and scales
def make_example(L, seed):
    R = random.Random(seed); g = lambda r, c, sd: [[R.gauss(0, sd) for _ in range(c)] for _ in range(r)]
    X = g(L, DM, 1.0); W = [g(DM, D, 1 / math.sqrt(DM)) for _ in range(3)]
    mm = lambda A, Bm: [[sum(A[i][t] * Bm[t][j] for t in range(len(Bm))) for j in range(len(Bm[0]))] for i in range(len(A))]
    Q, K, V = (mm(X, w) for w in W); q = Q[-1]
    sc = [sum(q[i] * K[p][i] for i in range(D)) / math.sqrt(D) for p in range(L)]; mx = max(sc); e = [math.exp(v - mx) for v in sc]; z = sum(e)
    out = [sum(e[p] / z * V[p][i] for p in range(L)) for i in range(D)]
    flat = lambda M: [v for r in M for v in r]
    sc_ = {"x": scale_for(flat(X)), "wq": scale_for(flat(W[0])), "wk": scale_for(flat(W[1])), "wv": scale_for(flat(W[2])), "q": scale_for(q), "k": scale_for(flat(K)), "v": scale_for(flat(V)), "o": scale_for(out)}
    return {"L": L, "X": X, "W": W, "out_float": out, "scales": sc_}
def rq_params(example):
    """(mantissa, shift) of every requantization in the program, from the scales."""
    s = example["scales"]; ms = mantissa_shift
    return {"q": ms(s["x"] * s["wq"] / s["q"]), "k": ms(s["x"] * s["wk"] / s["k"]), "v": ms(s["x"] * s["wv"] / s["v"]),
            "scores": ms(s["q"] * s["k"] / math.sqrt(D) * 16),            # int32 q.k  ->  real score x 16 (the softmax takes Q4.4)
            "weights": ms(127 / 65536),                                   # Q0.16 probability -> int8 weight in 0..127
            "out": ms(s["v"] / (127 * s["o"]))}                           # sum(w * v) -> int8 at the output scale
def ext_image(example):
    """External memory with X, the weights and (for 'cached') the K and V caches of the first L-1 tokens, all int8-quantized."""
    s = example["scales"]; L = example["L"]; img = [0] * I.EXT; w32 = lambda v: v & 0xFFFFFFFF
    for t in range(L):
        for c in range(DM): img[EX_X + t * DM + c] = w32(quantize(example["X"][t][c], s["x"]))
    for wi, (base, nm) in enumerate(((EX_WQ, "wq"), (EX_WK, "wk"), (EX_WV, "wv"))):
        for r in range(DM):
            for c in range(D): img[base + r * D + c] = w32(quantize(example["W"][wi][r][c], s[nm]))
    return img
# ---------------------------------------------------------------- the programs
def tail(L, P):
    """Attention over the K and V caches in the scratchpad (rows 0..L-1), for the q in SP_Q8; the result goes to ext[EX_OUT]."""
    prog = []
    for j in range((L + 3) // 4):
        n = min(4, L - 4 * j); prog.append(B("MM", dst=SP_S + 4 * j, A=SP_Q8, B=SP_KC + 4 * j * D, M=1, K=D, N=n, tb=1, lda=D, ldb=D, ldc=4))     # scores of 4 positions at a time
    prog.append(B("RQ", dst=SP_S8, src=SP_S, len=L, m=P["scores"][0], s=P["scores"][1]))
    prog.append(B("SM", dst=SP_P, src=SP_S8, len=L))
    prog.append(B("RQ", dst=SP_W, src=SP_P, len=L, m=P["weights"][0], s=P["weights"][1]))
    for c in range(D // 4):
        prog.append(B("MM", dst=SP_O + 4 * c, A=SP_W, B=SP_VC + 4 * c, M=1, K=L, N=4, tb=0, lda=L, ldb=D, ldc=4))                                # 4 columns of the output at a time
    prog.append(B("RQ", dst=SP_O8, src=SP_O, len=D, m=P["out"][0], s=P["out"][1]))
    prog.append(B("ST", src=SP_O8, dst=EX_OUT, len=D))
    return prog
def project(x_addr, w_addr, acc, rows, row_stride_acc):
    """MM instructions computing acc = x (rows x Dm) times w (Dm x d), 4 output columns at a time (and 4 rows at a time)."""
    out = []
    for i in range((rows + 3) // 4):
        m = min(4, rows - 4 * i)
        for j in range(D // 4): out.append(B("MM", dst=acc + 4 * i * row_stride_acc + 4 * j, A=x_addr + 4 * i * DM, B=w_addr + 4 * j, M=m, K=DM, N=4, tb=0, lda=DM, ldb=D, ldc=row_stride_acc))
    return out
def program(L, variant, P):
    prog = []
    if variant == "cached":
        prog += [B("LD", dst=SP_X, src=EX_X + (L - 1) * DM, len=DM), B("LD", dst=SP_WQ, src=EX_WQ, len=3 * DM * D)]
        if L > 1: prog += [B("LD", dst=SP_KC, src=EX_KC, len=(L - 1) * D), B("LD", dst=SP_VC, src=EX_VC, len=(L - 1) * D)]
        prog += project(SP_X, SP_WQ, SP_Q, 1, 4) + project(SP_X, SP_WK, SP_KN, 1, 4) + project(SP_X, SP_WV, SP_VN, 1, 4)
        prog += [B("RQ", dst=SP_Q8, src=SP_Q, len=D, m=P["q"][0], s=P["q"][1]),
                 B("RQ", dst=SP_KC + (L - 1) * D, src=SP_KN, len=D, m=P["k"][0], s=P["k"][1]), B("RQ", dst=SP_VC + (L - 1) * D, src=SP_VN, len=D, m=P["v"][0], s=P["v"][1]),
                 B("ST", src=SP_KC + (L - 1) * D, dst=EX_KC + (L - 1) * D, len=D), B("ST", src=SP_VC + (L - 1) * D, dst=EX_VC + (L - 1) * D, len=D)]       # append to the cache
    else:
        prog += [B("LD", dst=SP_X, src=EX_X, len=L * DM), B("LD", dst=SP_WQ, src=EX_WQ, len=3 * DM * D)]
        prog += project(SP_X, SP_WK, SP_ACC, L, D) + [B("RQ", dst=SP_KC, src=SP_ACC, len=L * D, m=P["k"][0], s=P["k"][1])]
        prog += project(SP_X, SP_WV, SP_ACC, L, D) + [B("RQ", dst=SP_VC, src=SP_ACC, len=L * D, m=P["v"][0], s=P["v"][1])]
        prog += project(SP_X + (L - 1) * DM, SP_WQ, SP_Q, 1, 4) + [B("RQ", dst=SP_Q8, src=SP_Q, len=D, m=P["q"][0], s=P["q"][1])]
    return prog + tail(L, P) + [B("HALT")]
def sequence(example):
    """Decode token by token from an EMPTY cache: step t (t = 1..L) attends over t tokens. The 'cached' program reads the caches that the previous steps appended to external memory
    (the external memory persists from step to step); the 'recompute' program ignores them. Returns, per step, both programs, the external memory each starts from, and both int8 outputs."""
    L = example["L"]; P = rq_params(example); ext = ext_image(example); steps = []
    for t in range(1, L + 1):
        prog_c = program(t, "cached", P); m_c = I.Machine(ext).run(prog_c)
        prog_r = program(t, "recompute", P); m_r = I.Machine(ext_image(example)).run(prog_r)
        out = lambda m: [I.s8(m.ext[EX_OUT + i]) for i in range(D)]
        steps.append({"t": t, "cached": (prog_c, list(ext), out(m_c)), "recompute": (prog_r, ext_image(example), out(m_r)), "spad": list(m_c.spad)})
        ext = m_c.ext
    return steps
def rel_error(example, out8):
    o = example["out_float"]; so = example["scales"]["o"]; d = [a * so - b for a, b in zip(out8, o)]
    return math.sqrt(sum(x * x for x in d) / D) / math.sqrt(sum(b * b for b in o) / D)
def int_reference(example, t):
    """The integer result of decode step t (t tokens), written straight from the maths with plain Python lists: no instructions, no tiles, no addresses. It shares only the arithmetic helpers
    (requantization, int8 matrix product, integer softmax) with the machine, so it checks the PROGRAM -- the tiling, strides, scales and the cache -- and not the arithmetic."""
    s = example["scales"]; P = rq_params(example)
    Xq = [[quantize(v, s["x"]) for v in r] for r in example["X"][:t]]
    Wq = [[[quantize(v, s[nm]) for v in r] for r in W] for W, nm in zip(example["W"], ("wq", "wk", "wv"))]
    rqm = lambda M, key: [[requant(a, *P[key]) for a in r] for r in M]
    q8 = rqm(matmul_int8([Xq[-1]], Wq[0]), "q")[0]; K8 = rqm(matmul_int8(Xq, Wq[1]), "k"); V8 = rqm(matmul_int8(Xq, Wq[2]), "v")
    scores = [requant(sum(q8[i] * K8[p][i] for i in range(D)), *P["scores"]) for p in range(t)]
    w = [requant(v, *P["weights"]) for v in softmax_fixed(scores)]
    return [requant(sum(w[p] * V8[p][c] for p in range(t)), *P["out"]) for c in range(D)]
def stagewise_problems(example, st):
    """Check every stage of a decode step against the REAL-NUMBER MEANING of its scale: the int8 values left in the scratchpad must equal the real-valued arithmetic on the previous stage's values,
    rounded, to within one level. It uses no mantissa/shift parameters and none of the builder's code, so it can see a wrong scale that an exact comparison with the same parameters cannot."""
    t = st["t"]; s = example["scales"]; sp = st["spad"]; probs = []
    rnd = lambda v: max(-127, min(127, int(math.floor(abs(v) + 0.5)) * (1 if v >= 0 else -1)))
    g = lambda base, n: [I.s32(sp[base + i]) for i in range(n)]
    Xq = [[quantize(v, s["x"]) for v in r] for r in example["X"][:t]]
    Wq = [[[quantize(v, s[nm]) for v in r] for r in W] for W, nm in zip(example["W"], ("wq", "wk", "wv"))]
    def stage(name, got, want):
        bad = sum(1 for a, b in zip(got, want) if abs(a - b) > 1)
        if bad: probs.append(f"step {t}: stage {name}: {bad} of {len(got)} values are more than one level from the real-number result")
    q8 = g(SP_Q8, D); stage("q", q8, [rnd(sum(Xq[-1][i] * Wq[0][i][c] for i in range(DM)) * s["x"] * s["wq"] / s["q"]) for c in range(D)])
    K8 = g(SP_KC, t * D); V8 = g(SP_VC, t * D)
    stage("k", K8, [rnd(sum(Xq[p][i] * Wq[1][i][c] for i in range(DM)) * s["x"] * s["wk"] / s["k"]) for p in range(t) for c in range(D)])
    stage("v", V8, [rnd(sum(Xq[p][i] * Wq[2][i][c] for i in range(DM)) * s["x"] * s["wv"] / s["v"]) for p in range(t) for c in range(D)])
    S8 = g(SP_S8, t); stage("scores", S8, [rnd(sum(q8[i] * s["q"] * K8[p * D + i] * s["k"] for i in range(D)) / math.sqrt(D) * 16) for p in range(t)])
    e = [math.exp((v - max(S8)) / 16) for v in S8]; z = sum(e)
    W8 = g(SP_W, t); stage("weights", W8, [rnd(v / z * 127) for v in e])
    O8 = g(SP_O8, D); stage("output", O8, [rnd(sum(W8[p] / 127 * V8[p * D + c] * s["v"] for p in range(t)) / s["o"]) for c in range(D)])
    return probs
def check_pipeline(seeds=range(4), Ls=(1, 2, 5, 9, 16, 24), bound=None, exact=False, stagewise=False):
    """Decode whole sequences both ways. At EVERY step the two programs' outputs must be identical (and, if `exact`, identical to the integer reference; if `stagewise`, every stage consistent with its real-number meaning), and at the last step the error against floating point must stay under the bound.
    Returns (worst error, problems)."""
    worst = 0.0; problems = []
    for L in Ls:
        for seed in seeds:
            ex = make_example(L, 100 * L + seed)
            for st in sequence(ex):
                if st["cached"][2] != st["recompute"][2]: problems.append(f"L={L} seed={seed} step {st['t']}: cached and recompute outputs differ")
                if exact and st["cached"][2] != int_reference(ex, st["t"]): problems.append(f"L={L} seed={seed} step {st['t']}: differs from the integer reference")
                if stagewise: problems += stagewise_problems(ex, st)
            e = rel_error(ex, st["cached"][2]); worst = max(worst, e)
            if bound is not None and e > bound: problems.append(f"L={L} seed={seed}: error {e:.3f} over the bound {bound}")
    return worst, problems
if __name__ == "__main__":
    w, pr = check_pipeline(exact=True, stagewise=True); print("worst relative error", round(w, 4), "problems", pr)
