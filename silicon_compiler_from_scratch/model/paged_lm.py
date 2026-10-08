#!/usr/bin/env python3
"""Chapter 15: a paged KV cache and sliding-window attention for the tiny language model of Chapter 12.
THE PAGE TABLE (host side). The cache is stored in fixed-size PAGES of `page` rows (one row = the key and value of one token). A request owns a BLOCK TABLE: the list of physical pages that hold its rows, in order. Pages come from a shared pool, so requests of different lengths share memory without a contiguous reservation per request. Pages can be SHARED between requests (a common prefix) with a reference count, and are copied before a shared page is written (copy-on-write). Pages that fall entirely outside the attention window are given back.
THE CHIP is oblivious to all of this: the host GATHERS the rows the window needs into the contiguous `kc`/`vc` inputs of the compiled program, exactly as before. With a window of W tokens the program for context W is the same for every later step, so the cost of a step stops growing.
"""
import math, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
os.environ.setdefault("GA2_EXT", "8192")
import capra as C, tiny_lm as T
class OutOfPages(Exception): pass
class PageTable:
    def __init__(self, n_pages, page=4):
        self.page = page; self.pool = [[None] * page for _ in range(n_pages)]; self.ref = [0] * n_pages; self.free = list(range(n_pages - 1, -1, -1))
        self.table = {}; self.length = {}; self.dropped = {}
    def new(self, rid): self.table[rid] = []; self.length[rid] = 0; self.dropped[rid] = 0
    def _alloc(self):
        if not self.free: raise OutOfPages("no free page")
        p = self.free.pop(); self.ref[p] = 1; return p
    def _unref(self, p):
        self.ref[p] -= 1
        if self.ref[p] == 0: self.free.append(p)
    def append(self, rid, row):
        i = self.length[rid]; off = i % self.page; t = self.table[rid]
        if off == 0: t.append(self._alloc())
        else:
            last = t[-1]
            if self.ref[last] > 1:                                  # copy on write: the page is shared, so write into a private copy
                q = self._alloc(); self.pool[q][:] = self.pool[last]; self._unref(last); t[-1] = q
        self.pool[t[-1]][off] = row; self.length[rid] = i + 1
    def rows(self, rid, lo, hi):
        """The rows with logical indices lo <= i < hi, gathered through the block table."""
        assert lo >= self.dropped[rid] * self.page, "rows already released"
        return [self.pool[self.table[rid][i // self.page - self.dropped[rid]]][i % self.page] for i in range(lo, hi)]
    def trim(self, rid, keep_from):
        """Release the pages that lie entirely before logical row keep_from."""
        t = self.table[rid]
        while t and (self.dropped[rid] + 1) * self.page <= keep_from: self._unref(t.pop(0)); self.dropped[rid] += 1
    def fork(self, src, dst):
        self.table[dst] = list(self.table[src]); self.length[dst] = self.length[src]; self.dropped[dst] = self.dropped[src]
        for p in self.table[dst]: self.ref[p] += 1
    def release(self, rid):
        for p in self.table.pop(rid): self._unref(p)
        del self.length[rid]; del self.dropped[rid]
    def used(self): return len(self.ref) - len(self.free)
def float_windowed(m, tok, kc, vc, W):
    """One float decode step that attends the new token and at most W-1 previous ones."""
    if W is not None and W >= 2 and len(kc) > W - 1: kc, vc = kc[-(W - 1):], vc[-(W - 1):]
    return T.float_step(m, tok, kc, vc)
def float_generate(m, start, n, W=None):
    toks = [start]; kc, vc = [], []
    for _ in range(n):
        lg, k, v, _ = float_windowed(m, toks[-1], kc, vc, W); kc.append(k); vc.append(v); toks.append(lg.index(max(lg)))
    return toks
def calibrate(m, steps, W, starts=range(T.V)):
    calib = {}; rk = rv = rx = 0.0
    for s in starts:
        toks = [s]; kc, vc = [], []
        for L in range(1, steps + 1):
            n = L if W is None else min(L, W); kk, vv = (kc[-(n - 1):], vc[-(n - 1):]) if n > 1 else ([], [])
            lg, k, v, _ = T.float_step(m, toks[-1], kk, vv); calib.setdefault(n, []).append({"x": [m["E"][toks[-1]]], **({"kc": [list(r) for r in kk], "vc": [list(r) for r in vv]} if n > 1 else {})})
            rk = max(rk, max(abs(t) for t in k)); rv = max(rv, max(abs(t) for t in v)); rx = max(rx, max(abs(t) for t in m["E"][toks[-1]])); kc.append(k); vc.append(v); toks.append(lg.index(max(lg)))
    return calib, {"x": rx, "k": rk, "v": rv}
class PagedChip:
    """The host: a page table for the cache, the window rule, one compiled program per context size actually used (min(L, W))."""
    def __init__(self, m, steps, W=None, page=4, n_pages=64, starts=range(T.V), paged=True):
        self.m = m; self.W = W; self.page = page; self.n_pages = n_pages; self.paged = paged; self.calib, self.ranges = calibrate(m, steps, W, starts); self.progs = {}
    def program(self, n):
        if n not in self.progs: self.progs[n] = C.compile_graph(T.build_graph(self.m, n), self.calib[n], ranges=self.ranges)
        return self.progs[n]
    def generate(self, start, n, force=None):
        """Decode n tokens. With paged=False the host keeps plain Python lists (the reference host); the two must give identical integers."""
        toks = [start]; rec = []; pt = PageTable(self.n_pages, self.page); pt.new(0); plain = []; peak = 0
        for L in range(1, n + 1):
            nc = L if self.W is None else min(L, self.W); lo = L - nc                       # rows lo .. L-2 are the previous tokens inside the window
            rows = pt.rows(0, lo, L - 1) if self.paged else plain[lo:L - 1]; inp = {"x": [self.m["E"][toks[-1]]]}
            if nc > 1: inp["kc"] = [list(r[0]) for r in rows]; inp["vc"] = [list(r[1]) for r in rows]
            P = self.program(nc); out, mach = C.run(P, inp, machine=True)
            rec.append({"L": L, "ctx": nc, "code": P.code, "ext": C.ext_image(P, inp), "logits": out["logits"][1][0], "ints": out["logits"][0][0], "tok": out["tok"][0][0][0], "pages": pt.used()})
            row = (out["k_new"][1][0], out["v_new"][1][0]); pt.append(0, row); plain.append(row)
            if self.W is not None: pt.trim(0, max(0, L + 1 - self.W))
            peak = max(peak, pt.used()); toks.append(force[L] if force else out["tok"][0][0][0])
        self.peak = peak; return toks, rec
if __name__ == "__main__":
    m = T.make_model("structured"); exp = [0]
    for _ in range(32): exp.append(T.f_next(exp[-1]))
    for W in (None, 8, 4):
        t, rec = PagedChip(m, 32, W).generate(0, 32); print(f"W={W}: rule ok {t == exp}; pages in use at the end {rec[-1]['pages']}")
