#!/usr/bin/env python3
"""Chapter 14: test the tests of the GQA model. Break model/gqa_lm.py one line at a time and run model/gqa_tests.py. 'caught' = some check reports a problem or the code crashes. Equivalent changes are listed separately."""
import concurrent.futures, os, shutil, subprocess, sys, tempfile
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); SRC = open(os.path.join(ROOT, "model", "gqa_lm.py")).read()
M = [
 ("float step: query head h reads group h % r instead of h // r", "g = h // r; qh = q[h * DH:(h + 1) * DH]", "g = h % r; qh = q[h * DH:(h + 1) * DH]"),
 ("float step: the scores are not divided by sqrt(d)", "for Kp in K]; mx = max(sc)", "for Kp in K]; sc = [s * math.sqrt(DH) for s in sc]; mx = max(sc)"),
 ("float step: the values come from the keys' columns", "Vv[p][g * DH + i] for p in range(len(K))", "K[p][g * DH + i] for p in range(len(K))"),
 ("conversion: keys are summed in a group instead of averaged", "sum(W[i][(g * r + t) * DH + c] for t in range(r)) / r for i in range(D)", "sum(W[i][(g * r + t) * DH + c] for t in range(r)) for i in range(D)"),
 ("conversion: only the first head of each group is kept", "sum(W[i][(g * r + t) * DH + c] for t in range(r)) / r for i in range(D)", "W[i][(g * r) * DH + c] for i in range(D)"),
 ("graph: query head h reads group h % r", "gi = h // r; q = g.matmul(x, w(f\"Wq{h}\"", "gi = h % r; q = g.matmul(x, w(f\"Wq{h}\""),
 ("graph: the query slice starts one column late", "sl(m[\"Wq\"], h * DH, (h + 1) * DH)", "sl(m[\"Wq\"], h * DH + 1, (h + 1) * DH + 1)"),
 ("graph: every group uses group 0's key weights", "sl(m[\"Wk\"], gi * DH, (gi + 1) * DH)", "sl(m[\"Wk\"], 0, DH)"),
 ("graph: the output projection rows are taken from the wrong head", "m[\"Wo\"][h * DH:(h + 1) * DH]", "m[\"Wo\"][0:DH]"),
 ("graph: the head outputs are not all added", "acc = o if acc is None else g.add(acc, o)", "acc = o if (acc is None or h == 3) else g.add(acc, o)"),
 ("graph: attention scale is 1/d instead of 1/sqrt(d)", "scale=1 / math.sqrt(DH))", "scale=1 / DH)"),
 ("host: the cache of group g is cut from the wrong columns", "inp[f\"kc{gi}\"] = [r[gi * DH:(gi + 1) * DH] for r in kc]; inp[f\"vc{gi}\"] = [r[gi * DH:(gi + 1) * DH] for r in vc]\n            out, mach", "inp[f\"kc{gi}\"] = [r[0:DH] for r in kc]; inp[f\"vc{gi}\"] = [r[gi * DH:(gi + 1) * DH] for r in vc]\n            out, mach"),
 ("host: the new value of each group is stored before its key", "kc.append([t for gi in range(G) for t in out[f\"k_new{gi}\"][1][0]]); vc.append([t for gi in range(G) for t in out[f\"v_new{gi}\"][1][0]])", "kc.append([t for gi in range(G) for t in out[f\"v_new{gi}\"][1][0]]); vc.append([t for gi in range(G) for t in out[f\"k_new{gi}\"][1][0]])"),
 ("cache size formula drops the factor 2 (keys and values)", "return 2 * G * DH", "return G * DH"),
]
EQUIV = [("EQUIVALENT in effect: the K and V cache are tagged separately but the host merges them (tags 'k' and 'v' share one range)", 'g.tag(g.input(f"vc{gi}", (L - 1, DH)), "v")', 'g.tag(g.input(f"vc{gi}", (L - 1, DH)), "v")')]
def check(label, old, new):
    if SRC.count(old) != 1: return label, "BAD ANCHOR (%d occurrences)" % SRC.count(old)
    d = tempfile.mkdtemp(prefix="mut14_"); shutil.copytree(os.path.join(ROOT, "model"), os.path.join(d, "model")); open(os.path.join(d, "model", "gqa_lm.py"), "w").write(SRC.replace(old, new))
    code = "import sys; sys.path.insert(0,'model'); import gqa_tests as T; p=T.check_all(); sys.exit(1 if p else 0)"
    try: r = subprocess.run([sys.executable, "-c", code], cwd=d, capture_output=True, text=True, timeout=600).returncode != 0
    except subprocess.TimeoutExpired: r = True
    shutil.rmtree(d, ignore_errors=True); return label, r
print("NOTE: this script deliberately breaks copies of the model. 'caught' is EXPECTED: it shows the checks notice the mistake.")
print("unbroken model passes:", check("base", "def mv(x, W)", "def mv(x, W)")[1] is False)
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool: rows = list(pool.map(lambda m: check(*m), M))
n = 0
for label, r in rows: print(f"{label}: {'caught' if r is True else r if isinstance(r, str) else 'NOT CAUGHT'}"); n += r is True
print(f"\nGQA model and graph: {n} of {len(M)} broken versions caught")
sys.exit(0 if n == len(M) else 1)
