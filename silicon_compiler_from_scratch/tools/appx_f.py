#!/usr/bin/env python3
"""Appendix F (memory systems and DRAM): a toy DRAM bank model and the arithmetic of bandwidth. All DRAM numbers are ASSUMED, typical of DDR4-class parts, to make the shapes of the effects visible; nothing here is a measurement of a real device. Usage: appx_f.py"""
import random
R = random.Random(2)
tRCD, tCL, tRP, tBURST = 14.0, 14.0, 14.0, 2.5          # ns: row to column delay (activate), column access (read latency), precharge (close a row), time on the bus for one 64-byte burst
ROW = 8192; LINE = 64; BANKS = 8
print("== 1. what a DRAM access costs: it depends on whether the row is already open (assumed timings in ns)")
print(f"  row hit  (the row is open):              column access {tCL} -> {tCL:.1f} ns to the first data")
print(f"  row empty (the bank is idle):            activate {tRCD} + column access {tCL} -> {tRCD + tCL:.1f} ns")
print(f"  row miss (another row is open):          precharge {tRP} + activate {tRCD} + column access {tCL} -> {tRP + tRCD + tCL:.1f} ns")
print(f"  one burst of {LINE} bytes then occupies the bus for {tBURST} ns: the peak bandwidth of one channel is {LINE / tBURST:.1f} GB/s")
class Dram:
    def __init__(self, banks=BANKS, policy="open"): self.open = [None] * banks; self.free = [0.0] * banks; self.bus = 0.0; self.policy = policy; self.hits = self.misses = self.empties = 0; self.banks = banks
    def access(self, addr, t):
        line = addr // LINE; bank = (line // (ROW // LINE)) % self.banks if False else line % self.banks; row = addr // (ROW * self.banks); start = max(t, self.free[bank])
        if self.open[bank] == row: lat = tCL; self.hits += 1
        elif self.open[bank] is None: lat = tRCD + tCL; self.empties += 1
        else: lat = tRP + tRCD + tCL; self.misses += 1
        self.open[bank] = row if self.policy == "open" else None; data = start + lat; busstart = max(data, self.bus); self.bus = busstart + tBURST; self.free[bank] = start + (lat - tCL) + tBURST if self.policy == "open" else start + lat + tRP; return self.bus
def run(addrs, banks=BANKS, policy="open", inflight=1):
    d = Dram(banks, policy); t = 0.0; lat = []; done = []
    for i, a in enumerate(addrs):
        issue = done[i - inflight] if i >= inflight else 0.0; fin = d.access(a, issue); done.append(fin); lat.append(fin - issue)
    total = done[-1]; return {"gbps": len(addrs) * LINE / total, "avg": sum(lat) / len(lat), "hits": d.hits / len(addrs), "total": total}
N = 4000
seq = [i * LINE for i in range(N)]; stride = [i * LINE * 64 for i in range(N)]; rnd = [R.randrange(0, 1 << 30) // LINE * LINE for _ in range(N)]
print("\n== 2. access patterns on the same DRAM: sequential (streaming a weight matrix), strided (column access), random (a gather). 8 banks, open-page policy")
print(f"  {'pattern':28s} {'one request at a time':>24s} {'8 requests in flight':>24s}")
for nm, ad in (("sequential 64-byte lines", seq), ("strided by 4 KiB", stride), ("random lines", rnd)):
    a, b = run(ad, inflight=1), run(ad, inflight=8); print(f"  {nm:28s} {a['gbps']:7.2f} GB/s ({100*a['hits']:3.0f}% row hits) {b['gbps']:7.2f} GB/s ({100*b['hits']:3.0f}% row hits)")
print("  the strided pattern puts every access in the same bank (bank = line number mod 8, and 4 KiB is exactly 64 lines): the row hits are as good as the stream's but there is no bank parallelism, so the throughput is lower")
print(f"  peak of the channel: {LINE / tBURST:.1f} GB/s. Streaming reaches a large part of it only with several requests in flight (latency hidden by parallelism); random access cannot, because most requests pay an activate")
print("\n== 3. open-page against closed-page policy (keep the row open after an access, or close it at once)")
print(f"  {'pattern':28s} {'open page':>12s} {'closed page':>12s}")
for nm, ad in (("sequential", seq), ("random", rnd)):
    print(f"  {nm:28s} {run(ad, inflight=8)['gbps']:9.2f} GB/s {run(ad, policy='closed', inflight=8)['gbps']:9.2f} GB/s")
print("  in this toy model open page wins for both patterns: for a stream by a factor of two (every access after the first is a row hit), for random access narrowly (a closed bank skips the precharge on the critical path but stays busy longer). Real controllers choose per workload or adaptively; the table shows only that the policy is a trade-off and that locality is what makes DRAM fast")
print("\n== 4. how many banks does it take? (sequential stream, 8 requests in flight)")
for b in (1, 2, 4, 8, 16): print(f"  {b:2d} banks: {run(seq, banks=b, inflight=8)['gbps']:6.2f} GB/s")
print("\n== 5. memories are a hierarchy: smaller is faster (typical orders of magnitude, ASSUMED; they differ by process and design)")
print(f"  {'level':24s} {'size':>12s} {'latency':>12s} {'bandwidth per unit':>20s}")
for r in (("registers", "~1 KB", "0 (same cycle)", "many TB/s"), ("on-chip SRAM scratchpad", "~1-100 MB", "1-10 ns", "TB/s (wide, close)"), ("HBM / GDDR / DDR DRAM", "~10-100 GB", "~50-100 ns", "25 GB/s (one DDR channel) to ~1 TB/s (HBM stack)"), ("flash", "~TB", "~100 us", "GB/s")): print(f"  {r[0]:24s} {r[1]:>12s} {r[2]:>12s} {r[3]:>20s}")
print("\n== 6. what the book's chips assume: Chapter 5's DMA has a FIXED latency (8 cycles) and one word per cycle. In this model that is the cost of a row hit followed by a stream; the real part is the pattern-dependent table above")
cycle = 1.0
print(f"  at a 1 ns cycle, the assumed row-hit access of {tCL:.0f} ns is {tCL / cycle:.0f} cycles; an 8-cycle latency is a faster, hit-only memory; the chapters' cycle counts are therefore OPTIMISTIC for DRAM and exact for the SRAM scratchpad")
print("\n== 7. bytes per token against bandwidth (Chapter 9): the ceiling on decode speed is bandwidth divided by the bytes read per token (derived)")
for nm, by in (("7B-class, fp16 weights (14 GB)", 14e9), ("7B-class, int8 (7 GB)", 7e9), ("7B-class, int4 (3.5 GB)", 3.5e9)):
    print(f"  {nm:32s}: " + "  ".join(f"{bw:5.0f} GB/s -> {bw * 1e9 / by:6.1f} tok/s" for bw in (50, 200, 1000)))
