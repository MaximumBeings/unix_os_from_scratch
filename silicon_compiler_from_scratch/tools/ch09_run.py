#!/usr/bin/env python3
"""Chapter 9: batching and the bandwidth analysis. Batched decode steps on the RTL (both simulators), cycles and traffic per batch size, the roofline of GA-2, and a derived table for a large model. Usage: ch09_run.py"""
import os, re, sys
os.environ["GA2_EXT"] = "8192"
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "model"))
import hw, ga2_isa as I, ga2_progs as G, batch as Bt
F = ["rtl/sram.v", "rtl/dma.v", "rtl/systolic.v", "rtl/requant.v", "rtl/exp_lut.v", "rtl/divu.v", "rtl/rowmem.v", "rtl/ga2_vec.v", "rtl/ga2_sm.v", "rtl/ga2_mm.v", "rtl/ga2.v", "tb/extmem_rw.v", "tb/ga2_tb.v"]
def line(out): return ([l for l in out.splitlines() if l.startswith(("PASS", "FAIL", "MISMATCH"))] or out.strip().splitlines()[-2:])[0]
print("== 1. batch invariance (Python): each sequence's result and cache are identical whether it is decoded alone or in a batch of 2, 3, 4, or in a ragged batch")
print("problems:", Bt.check_batching() or "none")
print("\n== 2. batched decode steps on the circuit: B sequences, all at their 16th token, then a ragged batch (lengths 3, 9, 1, 14)")
progs = []; meta = []
for nseq in (1, 2, 3, 4):
    ex = Bt.make_batch(nseq, 16, 4242); P = Bt.rq_params(ex); ext = Bt.ext_image(ex); seqs = list(range(nseq))
    for t in range(1, 16): ext = Bt.run(ext, Bt.program(seqs, [t] * nseq, P)).ext
    prog = Bt.program(seqs, [16] * nseq, P); progs.append((prog, ext)); meta.append((f"B={nseq} (all at token 16)", nseq, prog))
ex = Bt.make_batch(4, 16, 4343); P = Bt.rq_params(ex); ext = Bt.ext_image(ex); lens = [3, 9, 1, 14]
for b in range(4):
    for t in range(1, lens[b]): ext = Bt.run(ext, Bt.program([b], [t], P)).ext
prog = Bt.program([0, 1, 2, 3], lens, P); progs.append((prog, ext)); meta.append(("B=4 ragged (3, 9, 1, 14)", 4, prog))
words = [len(progs)]
for prog, ext in progs: words += G.record(prog, ext)
open(os.path.join(hw.ROOT, "out", "ga2_programs.hex"), "w").write("\n".join("%08x" % (w & 0xFFFFFFFF) for w in words) + "\n")
res = {}
for name, fn in (("Icarus", hw.sim_icarus), ("Verilator", hw.sim_verilator)):
    rc, out = fn(F, "ga2_tb", defines=("EXTW=8192", "DUMP")); print(f"  {name:10s}{line(out)} (exit {rc})")
    for m in re.finditer(r"COUNTERS (\d+) (\d+) (\d+) (\d+) (\d+) (\d+)", out): res[int(m.group(1))] = [int(g) for g in m.groups()[1:]]
print("\n== 3. cycles, traffic and the roofline.  Peak compute = 16 MAC/cycle (a 4x4 array). Memory = 1 word per cycle once a transfer is under way (one word = one int8 element).")
print(f"{'batch':26s} {'cycles':>7s} {'per token':>9s} {'MACs':>6s} {'words':>6s} {'MAC/word':>9s} {'achieved':>9s} {'ceiling':>8s} {'of ceiling':>10s} {'if loads overlapped':>20s}")
for i, (name, nseq, prog) in enumerate(meta):
    tot, mm, dma, vec, n = res[i]; macs, wds = Bt.traffic(prog); inten = macs / wds; ceil = min(16.0, inten * 1.0); ach = macs / tot
    ov = tot / max(dma, tot - dma)
    print(f"{name:26s} {tot:7d} {tot/nseq:9.0f} {macs:6d} {wds:6d} {inten:9.2f} {ach:9.3f} {ceil:8.2f} {100*ach/ceil:9.1f}% {ov:15.2f}x faster")
print("\n   ('per token' = cycles / number of sequences in the batch; 'achieved' = MACs / cycles; 'ceiling' = min(peak, MAC/word x words/cycle); the last column is the best case if every load ran in the shadow of compute, derived from the counters, not built)")
print("\n== 4. what batching can and cannot share (one step, L = 16 tokens per sequence; words from the programs, MACs counted from the MM instructions)")
macs_proj = 3 * 16 * 16; macs_att = 2 * 16 * 16; words_w = 3 * 16 * 16; words_seq = 16 + 2 * 15 * 16 + 2 * 16 + 16
print(f"shared by the batch  : weights {words_w} words; projections {macs_proj} MACs per sequence (these reuse the weights)")
print(f"per sequence         : cache and I/O {words_seq} words; attention {macs_att} MACs")
for nseq in (1, 2, 4, 16, 64, 1000000): print(f"  batch {nseq:>7d}: {(nseq*(macs_proj+macs_att))/(words_w+nseq*words_seq):5.2f} MAC/word")
print(f"  limit as the batch grows: {(macs_proj+macs_att)/words_seq:.2f} MAC/word at L = 16;  as L grows the limit tends to 1: every cached word is used once per step.")
print("\n== 5. derived, for a 7B-class model (7e9 weights; int8 cache of 256 KiB per token, 4096-token context = 1.07e9 bytes per sequence), ASSUMING a chip with 1 TB/s of memory bandwidth and ignoring compute and overheads")
print("   tokens per second per chip = B x BW / (weight bytes + B x cache bytes)")
KV = 2 * 32 * 32 * 128 * 1 * 4096; BW = 1e12
print(f"{'weights':14s} {'bytes':>8s} " + " ".join(f"{'B='+str(b):>8s}" for b in (1, 4, 16, 64, 256)) + f" {'cache = weights at B':>22s}")
for name, bpw in (("fp16", 2.0), ("int8", 1.0), ("int4", 0.5)):
    Wb = 7e9 * bpw; row = " ".join(f"{b*BW/(Wb+b*KV):8.0f}" for b in (1, 4, 16, 64, 256)); print(f"{name:14s} {Wb/1e9:6.1f}GB {row} {Wb/KV:22.1f}")
print(f"   upper limit as B grows (cache-bound): {BW/KV:.0f} tokens/s per chip at a 4096-token int8 cache")
