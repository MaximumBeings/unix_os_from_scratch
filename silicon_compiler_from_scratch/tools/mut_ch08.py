#!/usr/bin/env python3
"""Chapter 8: test the tests -- of the PROGRAM BUILDER (model/attn.py), the hand-written forerunner of the compiler. Each mutant breaks one line of the builder. Checks, each with (a) included:
  (a) cached and recompute programs must give identical outputs at every step of a decode loop;
  (b) (a) plus the error against floating point must stay under 12% (twice the worst error of the correct builder on these examples);
  (c) (a) plus the output must equal an integer reference written from the maths, exactly (it shares the scale parameters with the builder);
  (d) (a) plus every stage's int8 values must agree, within one level, with the real-number arithmetic its scale stands for (it shares nothing with the builder).
A mutant is 'caught' by a check if the check reports a problem or crashes."""
import os, shutil, subprocess, sys, tempfile, concurrent.futures
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); SRC = open(os.path.join(ROOT, "model", "attn.py")).read()
M = [
 ("scores: the 1/sqrt(d) factor is left out", 'sc["q"] * s["k"] / math.sqrt(D) * 16'.replace('sc[', 's['), 's["q"] * s["k"] * 16'),
 ("scores: the factor 16 (Q4.4 input of the softmax) is left out", 's["q"] * s["k"] / math.sqrt(D) * 16', 's["q"] * s["k"] / math.sqrt(D)'),
 ("weights: the factor 127 is left out", "ms(127 / 65536)", "ms(1 / 65536)"),
 ("output: the scale leaves out the 127", 'ms(s["v"] / (127 * s["o"]))', 'ms(s["v"] / s["o"])'),
 ("output: uses the key scale instead of the value scale", 'ms(s["v"] / (127 * s["o"]))', 'ms(s["k"] / (127 * s["o"]))'),
 ("q projection is requantized with the key weights' scale", '"q": ms(s["x"] * s["wq"] / s["q"])', '"q": ms(s["x"] * s["wk"] / s["q"])'),
 ("q requantization scale is 3% too large", '"q": ms(s["x"] * s["wq"] / s["q"])', '"q": ms(1.03 * s["x"] * s["wq"] / s["q"])'),
 ("output requantization scale is 2% too large", 'ms(s["v"] / (127 * s["o"]))', 'ms(1.02 * s["v"] / (127 * s["o"]))'),
 ("keys are read without the transpose", "M=1, K=D, N=n, tb=1, lda=D, ldb=D, ldc=4", "M=1, K=D, N=n, tb=0, lda=D, ldb=D, ldc=4"),
 ("values are read transposed", "M=1, K=L, N=4, tb=0, lda=L, ldb=D, ldc=4", "M=1, K=L, N=4, tb=1, lda=L, ldb=D, ldc=4"),
 ("the value matrix stride is 4 instead of d", "M=1, K=L, N=4, tb=0, lda=L, ldb=D, ldc=4", "M=1, K=L, N=4, tb=0, lda=L, ldb=4, ldc=4"),
 ("score tiles step through the keys by 4 words, not 4 rows", "B=SP_KC + 4 * j * D", "B=SP_KC + 4 * j"),
 ("the new key is written one row past the cache end", "dst=SP_KC + (L - 1) * D, src=SP_KN", "dst=SP_KC + L * D, src=SP_KN"),
 ("the new key is never appended to the cache", 'B("ST", src=SP_KC + (L - 1) * D, dst=EX_KC + (L - 1) * D, len=D), ', ""),
 ("the new value is appended to the key cache", "dst=EX_VC + (L - 1) * D, len=D)]", "dst=EX_KC + (L - 1) * D, len=D)]"),
 ("the key cache load is one row short", 'B("LD", dst=SP_KC, src=EX_KC, len=(L - 1) * D)', 'B("LD", dst=SP_KC, src=EX_KC, len=(L - 2) * D)'),
 ("the new token's hidden state is read from row 0", "src=EX_X + (L - 1) * DM, len=DM", "src=EX_X, len=DM"),
 ("the softmax covers one position too few", 'B("SM", dst=SP_P, src=SP_S8, len=L)', 'B("SM", dst=SP_P, src=SP_S8, len=L - 1)'),
 ("the weights are requantized with the scores' parameters", 'm=P["weights"][0], s=P["weights"][1]', 'm=P["scores"][0], s=P["scores"][1]'),
 ("the q projection uses the K weights", "project(SP_X, SP_WQ, SP_Q, 1, 4)", "project(SP_X, SP_WK, SP_Q, 1, 4)"),
 ("recompute: V is projected with the K weights", "project(SP_X, SP_WV, SP_ACC, L, D)", "project(SP_X, SP_WK, SP_ACC, L, D)"),
 ("the output requantization covers 8 of 16 values", 'B("RQ", dst=SP_O8, src=SP_O, len=D', 'B("RQ", dst=SP_O8, src=SP_O, len=8'),
 ("the output is stored from the int32 accumulators", 'B("ST", src=SP_O8, dst=EX_OUT', 'B("ST", src=SP_O, dst=EX_OUT'),
]
EQUIV = [
 ("EQUIVALENT: the last score tile always computes 4 positions (extra scores are never read)", "n = min(4, L - 4 * j)", "n = 4"),
 ("EQUIVALENT: the last projection block always computes 4 rows (extra rows are never read)", "m = min(4, rows - 4 * i)", "m = 4"),
]
def check(label, old, new, mode):
    if SRC.count(old) != 1: return label, "BAD ANCHOR (%d occurrences)" % SRC.count(old)
    d = tempfile.mkdtemp(prefix="mut8_"); shutil.copytree(os.path.join(ROOT, "model"), os.path.join(d, "model")); open(os.path.join(d, "model", "attn.py"), "w").write(SRC.replace(old, new))
    code = "import sys; sys.path.insert(0,'model'); import attn; w,p=attn.check_pipeline(bound=%s, exact=%s, stagewise=%s); sys.exit(1 if p else 0)" % ("0.12" if mode == "b" else "None", mode == "c", mode == "d")
    r = subprocess.run([sys.executable, "-c", code], cwd=d, capture_output=True, text=True, timeout=300); shutil.rmtree(d, ignore_errors=True)
    return label, r.returncode != 0
def table(muts):
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        r = {md: list(pool.map(lambda m: check(m[0], m[1], m[2], md), muts)) for md in "bcd"}
    return [(r["b"][i][0], r["b"][i][1], r["c"][i][1], r["d"][i][1]) for i in range(len(muts))]
print("NOTE: this script deliberately breaks copies of the program builder. 'caught' is EXPECTED: it shows the checks notice the mistake.")
print(f"{'':66s} {'(b) accuracy':>13s} {'(c) exact':>11s} {'(d) stagewise':>14s}")
rows = table(M); tot = [sum(1 for r in rows if r[k] is True) for k in (1, 2, 3)]; cw = lambda v: "caught" if v is True else "NOT CAUGHT"
for label, b, c, d in rows: print(f"{label:66s} {cw(b):>13s} {cw(c):>11s} {cw(d):>14s}")
print(f"\ncaught: (b) accuracy bound {tot[0]} of {len(M)};  (c) exact integer reference {tot[1]} of {len(M)};  (d) stage-wise real-number check {tot[2]} of {len(M)};  by any of them {sum(1 for r in rows if True in r[1:])} of {len(M)}")
print("\nChanges that look like bugs and are not:")
for label, b, c, d in table(EQUIV): print(f"{label}: {'caught' if True in (b, c, d) else 'NOT CAUGHT'}")
sys.exit(0 if all(True in r[1:] for r in rows) else 1)
