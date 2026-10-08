#!/usr/bin/env python3
"""Chapter 16: test the tests of speculative decoding. Break model/spec_lm.py one line at a time and run model/spec_tests.py. 'caught' = a check reports a problem or the code crashes."""
import concurrent.futures, os, shutil, subprocess, sys, tempfile
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); SRC = open(os.path.join(ROOT, "model", "spec_lm.py")).read()
M = [
 ("accept rule: one more draft token is accepted than agreed", "while a < k and d[a] == choice[a]: a += 1", "while a < k and d[a] == choice[a]: a += 1\n        a = min(k, a + 1) if a < k else a"),
 ("accept rule: the first guess is not checked", "while a < k and d[a] == choice[a]: a += 1", "a = 1 if k > 1 and d[0] != choice[0] else 0\n        while a < k and d[a] == choice[a]: a += 1"),
 ("accept rule: guess i is compared with the target's choice after row i+1", "while a < k and d[a] == choice[a]: a += 1", "while a < k and d[a] == choice[a + 1]: a += 1"),
 ("accept rule: acceptance stops at the first agreement, not the first disagreement", "while a < k and d[a] == choice[a]: a += 1", "while a < k and d[a] != choice[a]: a += 1"),
 ("bonus token: the draft's token is kept instead of the target's", "seq += d[:a] + [choice[a]]", "seq += d[:a] + [d[a] if a < k else choice[a]]"),
 ("bonus token: taken from the previous row", "seq += d[:a] + [choice[a]]", "seq += d[:a] + [choice[max(0, a - 1)]]"),
 ("bonus token: not appended", "seq += d[:a] + [choice[a]]", "seq += d[:a] + ([choice[a]] if a == 0 else [])"),
 ("cache: the rows of rejected guesses are kept", "kc += ks[:a + 1]; vc += vs[:a + 1]", "kc += ks; vc += vs"),
 ("cache: the row of the bonus token's input is dropped", "kc += ks[:a + 1]; vc += vs[:a + 1]", "kc += ks[:max(1, a)]; vc += vs[:max(1, a)]"),
 ("cache: values and keys swapped", "kc += ks[:a + 1]; vc += vs[:a + 1]", "kc += vs[:a + 1]; vc += ks[:a + 1]"),
 ("block: the last committed token is left out of the block", "block = [seq[-1]] + d;", "block = d + [seq[-1]];"),
 ("draft chain: every guess is made from the committed token, not the previous guess", "for _ in range(k): t = draft(t); d.append(t)", "for _ in range(k): t = draft(seq[-1]); d.append(t)"),
 ("verifier mask: a row may see one later row", "Ki = g.slice_rows(K, 0, c + i + 1); Vi = g.slice_rows(Vv, 0, c + i + 1)", "Ki = g.slice_rows(K, 0, min(c + m, c + i + 2)); Vi = g.slice_rows(Vv, 0, min(c + m, c + i + 2))"),
 ("verifier mask: a row cannot see itself", "Ki = g.slice_rows(K, 0, c + i + 1); Vi = g.slice_rows(Vv, 0, c + i + 1)", "Ki = g.slice_rows(K, 0, max(1, c + i)); Vi = g.slice_rows(Vv, 0, max(1, c + i))"),
 ("verifier mask: keys see the prefix but values see everything", "Vi = g.slice_rows(Vv, 0, c + i + 1)", "Vi = g.slice_rows(Vv, 0, c + m)"),
 ("verifier: the query of row i is row 0's", "qi = g.slice_rows(Q, i, 1)", "qi = g.slice_rows(Q, 0, 1)"),
 ("verifier: the rows of the attention output are concatenated in reverse", "A = ai if A is None else g.concat_rows(A, ai)", "A = ai if A is None else g.concat_rows(ai, A)"),
 ("verifier float block: the mask lets row i see the whole block", "if j <= c + i else -1e30", "if j <= c + m - 1 else -1e30"),
 ("draft table: the rule's next token is f(t)+1", "L[t][T.f_next(t)] = 8.0", "L[t][(T.f_next(t) + 1) % 16] = 8.0"),
]
def check(label, old, new):
    if SRC.count(old) != 1: return label, "BAD ANCHOR (%d occurrences)" % SRC.count(old)
    d = tempfile.mkdtemp(prefix="mut16_"); shutil.copytree(os.path.join(ROOT, "model"), os.path.join(d, "model")); open(os.path.join(d, "model", "spec_lm.py"), "w").write(SRC.replace(old, new))
    code = "import sys; sys.path.insert(0,'model'); import spec_tests as T; p=T.check_all(); sys.exit(1 if p else 0)"
    try: r = subprocess.run([sys.executable, "-c", code], cwd=d, capture_output=True, text=True, timeout=900).returncode != 0
    except subprocess.TimeoutExpired: r = True
    shutil.rmtree(d, ignore_errors=True); return label, r
print("NOTE: this script deliberately breaks copies of the model. 'caught' is EXPECTED: it shows the checks notice the mistake.")
print("unbroken model passes:", check("base", "def solve(A, B)", "def solve(A, B)")[1] is False)
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool: rows = list(pool.map(lambda m: check(*m), M))
n = 0
for label, r in rows: print(f"{label}: {'caught' if r is True else r if isinstance(r, str) else 'NOT CAUGHT'}"); n += r is True
print(f"\nspeculative decoding: {n} of {len(M)} broken versions caught")
sys.exit(0 if n == len(M) else 1)
