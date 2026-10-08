#!/usr/bin/env python3
"""Chapter 17: test the tests of the mixture-of-experts block. Break model/moe_lm.py one line at a time and run model/moe_tests.py. 'caught' = a check reports a problem or the code crashes. Equivalent changes are listed separately."""
import concurrent.futures, os, shutil, subprocess, sys, tempfile
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); SRC = open(os.path.join(ROOT, "model", "moe_lm.py")).read()
M = [
 ("dispatch: the host runs the next expert", "def dispatch(self, route): return route", "def dispatch(self, route): return (route + 1) % E"),
 ("dispatch: the host always runs expert 0", "def dispatch(self, route): return route", "def dispatch(self, route): return 0"),
 ("router: the float step takes the least likely expert", "e = r.index(max(r)) if route is None else route", "e = r.index(min(r)) if route is None else route"),
 ("router: the router weight is applied to x instead of h", "r = g.matmul(h, w(\"Wr\", m[\"Wr\"]))", "r = g.matmul(x, w(\"Wr\", m[\"Wr\"]))"),
 ("router: the router reads the wrong columns (two experts swapped)", "r = g.matmul(h, w(\"Wr\", m[\"Wr\"]))", "r = g.matmul(h, w(\"Wr\", [[row[1], row[0]] + row[2:] for row in m[\"Wr\"]]))"),
 ("expert: the second matrix comes from the next expert", "h = g.tag(g.input(\"h\", (rows, D)), \"h\"); W1, W2 = m[\"experts\"][e]", "h = g.tag(g.input(\"h\", (rows, D)), \"h\"); W1, W2 = m[\"experts\"][e][0], m[\"experts\"][(e + 1) % E][1]"),
 ("expert: the residual connection is left out of B", "y = g.add(h, g.matmul(g.relu(g.matmul(h, g.weight(\"W1\", W1))), g.weight(\"W2\", W2))); lg", "y = g.matmul(g.relu(g.matmul(h, g.weight(\"W1\", W1))), g.weight(\"W2\", W2)); lg"),
 ("expert: the relu is left out of B", "g.matmul(g.relu(g.matmul(h, g.weight(\"W1\", W1))), g.weight(\"W2\", W2))); lg", "g.matmul(g.matmul(h, g.weight(\"W1\", W1)), g.weight(\"W2\", W2))); lg"),
 ("expert: the float step uses expert 0's weights", "W1, W2 = m[\"experts\"][e]; y = [hi", "W1, W2 = m[\"experts\"][0]; y = [hi"),
 ("hand-over: program B gets the integer h instead of the real h", "ob = C.run(PB, {\"h\": h})", "ob = C.run(PB, {\"h\": oa[\"h\"][0]})"),
 ("hand-over: program B's external memory is built from a different h", "C.ext_image(PB, {\"h\": h})}]})", "C.ext_image(PB, {\"h\": [[0.0] * D]})}]})"),
 ("hand-over: the router's choice is read from the wrong output", "route = oa[\"route\"][0][0][0]", "route = oa[\"route\"][0][0][0] if L % 5 else 0"),
 ("structured model: the experts are assigned to the wrong groups", "members = [t for t in range(V) if group(t) == e]", "members = [t for t in range(V) if group(t) == (e + 1) % E]"),
 ("structured model: an expert's output does not subtract the input", "(Em[T.f_next(t)][d] - Em[t][d]) / cc", "(Em[T.f_next(t)][d]) / cc"),
 ("cache: the new key and value are swapped on the way back", "kc.append(oa[\"k_new\"][1][0]); vc.append(oa[\"v_new\"][1][0])", "kc.append(oa[\"v_new\"][1][0]); vc.append(oa[\"k_new\"][1][0])"),
 ("dense comparison: only expert 0 is added", "for e in range(E): y = g.add(y, g.matmul(g.relu(g.matmul(h, w(f\"W1_{e}\"", "for e in range(1): y = g.add(y, g.matmul(g.relu(g.matmul(h, w(f\"W1_{e}\""),
]
EQUIV = [("EQUIVALENT in effect: the record reports the router's choice as the expert that ran (the same thing whenever the dispatch is right; a wrong dispatch is caught by the tokens and by the check of the program that ran)", "\"route\": route, \"expert\": e,", "\"route\": route, \"expert\": route,"),
 ("NOT EXERCISED (every expert is used in the calibration runs, so the fallback never runs; a test that left an expert unused would exercise it)", "self.cb[e] = allh[:8]", "self.cb[e] = allh[:7]")]
def check(label, old, new):
    if SRC.count(old) != 1: return label, "BAD ANCHOR (%d occurrences)" % SRC.count(old)
    d = tempfile.mkdtemp(prefix="mut17_"); shutil.copytree(os.path.join(ROOT, "model"), os.path.join(d, "model")); open(os.path.join(d, "model", "moe_lm.py"), "w").write(SRC.replace(old, new))
    code = "import sys; sys.path.insert(0,'model'); import moe_tests as T; p=T.check_all(); sys.exit(1 if p else 0)"
    try: r = subprocess.run([sys.executable, "-c", code], cwd=d, capture_output=True, text=True, timeout=900).returncode != 0
    except subprocess.TimeoutExpired: r = True
    shutil.rmtree(d, ignore_errors=True); return label, r
print("NOTE: this script deliberately breaks copies of the model. 'caught' is EXPECTED: it shows the checks notice the mistake.")
print("unbroken model passes:", check("base", "def group(t)", "def group(t)")[1] is False)
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool: rows = list(pool.map(lambda m: check(*m), M)); eq = list(pool.map(lambda m: check(*m), EQUIV))
n = 0
for label, r in rows: print(f"{label}: {'caught' if r is True else r if isinstance(r, str) else 'NOT CAUGHT'}"); n += r is True
print(f"\nmixture of experts: {n} of {len(M)} broken versions caught")
print("\nChanges that look like bugs and are not:")
for label, r in eq: print(f"{label}: {'caught' if r is True else 'NOT CAUGHT'}")
sys.exit(0 if n == len(M) else 1)
