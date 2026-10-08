#!/usr/bin/env python3
"""Chapter 16: speculative decoding. (1) the software checks; (2) 32 tokens of the tiny model decoded plainly and speculatively (several draft qualities, two target models), every program executed replayed on the RTL in both simulators. Usage: ch16_run.py"""
import os, re, sys
os.environ["GA2_EXT"] = "8192"
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "model"))
import hw, ga2_progs as G_, ga2_isa as I, tiny_lm as T, spec_lm as S, spec_tests as ST
F = ["rtl/sram.v", "rtl/dma.v", "rtl/systolic.v", "rtl/requant.v", "rtl/exp_lut.v", "rtl/divu.v", "rtl/rowmem.v", "rtl/ga2_vec.v", "rtl/ga2_sm.v", "rtl/ga2_mm.v", "rtl/ga2.v", "tb/extmem_rw.v", "tb/ga2_tb.v"]
def line(out): return ([l for l in out.splitlines() if l.startswith(("PASS", "FAIL", "MISMATCH"))] or out.strip().splitlines()[-2:])[0]
N = 32
print("== 1. software checks: block form == sequential float step; exactness for every draft; causality on the chip (integer-identical rows); attention/keys/values/logits close to float; the rule through the chip's loop")
p = ST.check_all(); print("  problems:", p if p else "none")
def replay(label, log):
    words = [len(log)]
    for r in log: words += G_.record(r["code"], r["ext"])
    open(os.path.join(hw.ROOT, "out", "ga2_programs.hex"), "w").write("\n".join("%08x" % (w & 0xFFFFFFFF) for w in words) + "\n"); cnt = {}
    for name, fn in (("Icarus", hw.sim_icarus), ("Verilator", hw.sim_verilator)):
        rc, out = fn(F, "ga2_tb", defines=("EXTW=8192", "DUMP")); print(f"  {label:34s}{name:10s}{line(out)} (exit {rc})")
        cnt[name] = [int(mm.group(2)) for mm in re.finditer(r"COUNTERS (\d+) (\d+) (\d+) (\d+) (\d+) (\d+)", out)]
    assert cnt["Icarus"] == cnt["Verilator"]; return sum(cnt["Icarus"]), cnt["Icarus"]
print("\n== 2. 32 tokens, structured model: plain greedy decoding on the chip, then speculative decoding with k = 3 and drafts of different quality")
m = T.make_model("structured"); exp = [0]
for _ in range(N): exp.append(T.f_next(exp[-1]))
chip = T.Chip(m, N); tb, rec = chip.generate(0, N); log = [{"code": r["code"], "ext": r["ext"]} for r in rec]; base, _ = replay("plain decoding (32 programs)", log)
rows = [("plain greedy decoding", tb == exp, "-", "-", len(log), base)]
for label, noise in (("draft: perfect (noise 0)", 0.0), ("draft: noisy (noise 3)", 3.0), ("draft: poor (noise 4)", 4.0)):
    Wd = S.make_draft(m, noise, 1); sc = S.SpecChip(m, Wd, N, 3); toks, st, lg = sc.generate(0, N); cyc, _ = replay(f"speculative k=3, {label[7:]}", lg)
    assert cyc == S.cycles_of(lg), "RTL cycles differ from the model's"
    rows.append((f"speculative k=3, {label}", toks == exp, f"{st['accepted']}/{st['proposed']}", st["blocks"], len(lg), cyc))
print("\n== 3. a generic target (random weights): the draft is a bigram table fitted to the target's own output; compare with plain decoding ON THE CHIP")
mr = T.make_model("random", 1); chr_ = T.Chip(mr, N); tr, recr = chr_.generate(0, N); logr = [{"code": r["code"], "ext": r["ext"]} for r in recr]; baser, _ = replay("plain decoding, random model", logr)
rows.append(("random model: plain decoding", True, "-", "-", len(logr), baser))
for k in (1, 3):
    Wd = S.make_draft(mr, 0.0, 1); sc = S.SpecChip(mr, Wd, N, k); toks, st, lg = sc.generate(0, N); cyc, _ = replay(f"speculative k={k}, random model", lg)
    rows.append((f"random model: speculative k={k}", toks == tr, f"{st['accepted']}/{st['proposed']}", st["blocks"], len(lg), cyc))
print(f"\n{'run':44s} {'tokens equal':>12s} {'accepted':>9s} {'blocks':>7s} {'programs':>9s} {'RTL cycles':>11s} {'speedup':>8s}")
for r in rows:
    b = base if "random" not in r[0] else baser; print(f"{r[0]:44s} {str(r[1]):>12s} {r[2]:>9s} {str(r[3]):>7s} {r[4]:9d} {r[5]:11d} {b/r[5]:7.2f}x")
print("  ('tokens equal' compares with the rule for the structured model, and with the chip's own plain greedy tokens for the random model)")
