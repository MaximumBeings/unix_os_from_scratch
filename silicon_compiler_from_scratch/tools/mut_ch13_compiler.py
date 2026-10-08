#!/usr/bin/env python3
"""Chapter 13: test the tests of the compiler's int4 path. Break model/capra.py one line at a time and run the int4 battery: 60 random graphs whose weights are all int4 (checks (a)-(d) with error bound 0.72) plus the refusal of an unsupported bit width. 'caught' = some check reports a problem or the compiler crashes."""
import concurrent.futures, os, shutil, subprocess, sys, tempfile
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); SRC = open(os.path.join(ROOT, "model", "capra.py")).read()
M = [
 ("scale: int4 weights use range/8 instead of range/7", "(rng[n.id] if rng[n.id] > 0 else 1.0) / 7", "(rng[n.id] if rng[n.id] > 0 else 1.0) / 8"),
 ("packing: the fields are stored in reverse order", "(flat[8 * i + j] & 15) << (4 * j)", "(flat[8 * i + j] & 15) << (4 * (7 - j))"),
 ("packing: the fields are not masked to 4 bits", "(flat[8 * i + j] & 15) << (4 * j)", "(flat[8 * i + j]) << (4 * j)"),
 ("packing: the last partial word is not padded", "flat += [0] * (-len(flat) % 8)", "pass"),
 ("layout: the packed length rounds down", "words = (n.shape[0] * n.shape[1] + 7) // 8 if", "words = (n.shape[0] * n.shape[1]) // 8 if"),
 ("lowering: the UNPACK is left out", "code.append(B(\"UNPACK\", dst=a, src=tmp, len=pw)); ", ""),
 ("lowering: the UNPACK handles one word too few", "code.append(B(\"UNPACK\", dst=a, src=tmp, len=pw))", "code.append(B(\"UNPACK\", dst=a, src=tmp, len=max(1, pw - 1)))"),
 ("lowering: only one packed word is loaded", "code.append(B(\"LD\", dst=tmp, src=e, len=pw))", "code.append(B(\"LD\", dst=tmp, src=e, len=1))"),
 ("layout: buffers of int4 weights are not rounded up to a multiple of 8", "((n.shape[0] * n.shape[1] + 7) // 8) * 8 if n.op == \"weight\"", "(n.shape[0] * n.shape[1]) if n.op == \"weight\""),
 ("checks: a bit width of 5 is accepted by weight()", "if bits not in (4, 8): raise", "if bits not in (4, 5, 8): raise"),
 ("lowering: UNPACK reads the weight's own buffer, not the loaded words", "B(\"UNPACK\", dst=a, src=tmp,", "B(\"UNPACK\", dst=a, src=a,"),
]
EQUIV = [("NOT EQUIVALENT but UNTESTED (a gap): the staging area for packed words is never given back", "alloc.release(tmp, pw)", "pass")]
CODE = ("import sys; sys.path.insert(0,'model'); import capra as C, capra_tests as T\n"
        "p, w = T.check_many(range(60), bound=0.72, wbits=4); bad = bool(p)\n"
        "try:\n    C.Graph().weight('w', [[1.0]], bits=5); bad = True\nexcept C.CompileError: pass\n"
        "sys.exit(1 if bad else 0)")
def check(label, old, new):
    if SRC.count(old) != 1: return label, "BAD ANCHOR (%d occurrences)" % SRC.count(old)
    d = tempfile.mkdtemp(prefix="mut13c_"); shutil.copytree(os.path.join(ROOT, "model"), os.path.join(d, "model")); open(os.path.join(d, "model", "capra.py"), "w").write(SRC.replace(old, new))
    try: r = subprocess.run([sys.executable, "-c", CODE], cwd=d, capture_output=True, text=True, timeout=900).returncode != 0
    except subprocess.TimeoutExpired: r = True
    shutil.rmtree(d, ignore_errors=True); return label, r
print("NOTE: this script deliberately breaks copies of the compiler. 'caught' is EXPECTED: it shows the int4 battery notices the mistake.")
print("unbroken compiler passes the battery:", check("base", "def weight(self", "def weight(self")[1] is False)
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool: rows = list(pool.map(lambda m: check(*m), M)); eq = list(pool.map(lambda m: check(*m), EQUIV))
n = 0
for label, r in rows: print(f"{label}: {'caught' if r is True else r if isinstance(r, str) else 'NOT CAUGHT'}"); n += r is True
print(f"\nint4 compiler path: {n} of {len(M)} broken compilers caught")
print("\nA real gap, reported not counted:")
for label, r in eq: print(f"{label}: {'caught' if r is True else r if isinstance(r, str) else 'NOT CAUGHT'}")
sys.exit(0 if n == len(M) else 1)
