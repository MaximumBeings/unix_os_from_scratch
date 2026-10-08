#!/usr/bin/env python3
"""Chapter 15: test the tests of the page table, window rule and paged host. Break model/paged_lm.py one line at a time and run model/paged_tests.py. 'caught' = a check reports a problem or the code crashes."""
import concurrent.futures, os, shutil, subprocess, sys, tempfile
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); SRC = open(os.path.join(ROOT, "model", "paged_lm.py")).read()
M = [
 ("page table: a page freed by its last owner is not returned to the free list", "if self.ref[p] == 0: self.free.append(p)", "if self.ref[p] == 0: pass"),
 ("page table: a freed page is put on the free list while others still share it", "if self.ref[p] == 0: self.free.append(p)", "if self.ref[p] <= 1: self.free.append(p)"),
 ("page table: fork does not count the new owner", "for p in self.table[dst]: self.ref[p] += 1", "pass"),
 ("page table: no copy before writing into a shared page", "if self.ref[last] > 1:", "if False:"),
 ("page table: the copy-on-write copy is empty", "self.pool[q][:] = self.pool[last]; self._unref(last)", "self._unref(last)"),
 ("page table: the copy keeps the shared page's reference", "self.pool[q][:] = self.pool[last]; self._unref(last); t[-1] = q", "self.pool[q][:] = self.pool[last]; t[-1] = q"),
 ("page table: a new page is taken from a full page's neighbour (offset uses page+1)", "i = self.length[rid]; off = i % self.page; t = self.table[rid]", "i = self.length[rid]; off = i % (self.page + 1) if self.page > 1 else 0; t = self.table[rid]"),
 ("page table: the block-table lookup ignores pages already dropped", "self.table[rid][i // self.page - self.dropped[rid]]", "self.table[rid][i // self.page]"),
 ("page table: trim releases a page that still holds a needed row", "while t and (self.dropped[rid] + 1) * self.page <= keep_from:", "while t and (self.dropped[rid] + 1) * self.page <= keep_from + 1:"),
 ("page table: trim keeps pages that are completely outside the window", "while t and (self.dropped[rid] + 1) * self.page <= keep_from:", "while t and (self.dropped[rid] + 2) * self.page <= keep_from:"),
 ("page table: release forgets the request's pages", "for p in self.table.pop(rid): self._unref(p)", "self.table.pop(rid)"),
 ("page table: exhaustion is not reported", 'if not self.free: raise OutOfPages("no free page")', "if not self.free: return 0"),
 ("window: the float reference keeps W rows, not W-1", "kc, vc = kc[-(W - 1):], vc[-(W - 1):]", "kc, vc = kc[-W:], vc[-W:]"),
 ("window: the chip's context is W+1", "nc = L if self.W is None else min(L, self.W); lo = L - nc", "nc = L if self.W is None else min(L, self.W + 1); lo = L - nc"),
 ("host: the gather starts one row late", "rows = pt.rows(0, lo, L - 1) if self.paged else plain[lo:L - 1]", "rows = pt.rows(0, lo + 1, L - 1) if self.paged else plain[lo:L - 1]"),
 ("host: the gather returns values where keys belong", 'inp["kc"] = [list(r[0]) for r in rows]; inp["vc"] = [list(r[1]) for r in rows]', 'inp["kc"] = [list(r[1]) for r in rows]; inp["vc"] = [list(r[0]) for r in rows]'),
 ("host: pages outside the window are never released", "if self.W is not None: pt.trim(0, max(0, L + 1 - self.W))", "pass"),
 ("host: the trim point is one token too late", "pt.trim(0, max(0, L + 1 - self.W))", "pt.trim(0, max(0, L - self.W))"),
]
def check(label, old, new):
    if SRC.count(old) != 1: return label, "BAD ANCHOR (%d occurrences)" % SRC.count(old)
    d = tempfile.mkdtemp(prefix="mut15_"); shutil.copytree(os.path.join(ROOT, "model"), os.path.join(d, "model")); open(os.path.join(d, "model", "paged_lm.py"), "w").write(SRC.replace(old, new))
    code = "import sys; sys.path.insert(0,'model'); import paged_tests as T; p=T.check_all(); sys.exit(1 if p else 0)"
    try: r = subprocess.run([sys.executable, "-c", code], cwd=d, capture_output=True, text=True, timeout=600).returncode != 0
    except subprocess.TimeoutExpired: r = True
    shutil.rmtree(d, ignore_errors=True); return label, r
print("NOTE: this script deliberately breaks copies of the model. 'caught' is EXPECTED: it shows the checks notice the mistake.")
print("unbroken model passes:", check("base", "def float_windowed", "def float_windowed")[1] is False)
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool: rows = list(pool.map(lambda m: check(*m), M))
n = 0
for label, r in rows: print(f"{label}: {'caught' if r is True else r if isinstance(r, str) else 'NOT CAUGHT'}"); n += r is True
print(f"\npage table, window and host: {n} of {len(M)} broken versions caught")
sys.exit(0 if n == len(M) else 1)
