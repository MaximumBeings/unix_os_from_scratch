#!/usr/bin/env python3
"""GA-2: the instruction set of the accelerator, its assembler, and a Python reference simulator (the answer key for the circuit).

MEMORIES. External memory ("ext", DRAM) and the scratchpad ("spad", 4096 words) both hold 32-bit words. Every value occupies one word: an int8 is stored sign-extended, an int32 accumulator fills the word, a Q0.16 probability is 0..65536.
(The circuit stores one element per word to keep the memory system simple; the bandwidth analysis of Chapter 9 counts BYTES of the logical type, not words.)
INSTRUCTION. 128 bits: op[127:124] then fields a[123:108] b[107:92] c[91:76] d[75:52] e[51:46] f[45:38] g[37:30] h[29:22] fl[21:18] x[17:0].
  op 0 HALT
  op 1 LD   a=spad dst, b=ext src, c=len                 spad[a+i] = ext[b+i]
  op 2 ST   a=spad src, b=ext dst, c=len                 ext[b+i]  = spad[a+i]
  op 3 MM   a=dst, b=A, c=B, f=M, g=K, h=N, fl0=tb, d[11:0]=lda, d[23:12]=ldb, x[11:0]=ldc
            C[m][n] = sum_k A[m][k] * B[k][n] (int8 inputs, int32 sums, wrap-around);  A[m][k]=spad[b+m*lda+k];  B[k][n]=spad[c+k*ldb+n] (tb=0) or spad[c+n*ldb+k] (tb=1);  spad[a+m*ldc+n]=C[m][n].  M,N in 1..4, K in 1..64
  op 4 RQ   a=dst, b=src, c=len, d=mantissa, e=shift, fl0=relu       spad[a+i] = requant(int32 spad[b+i])    (Chapter 3)
  op 5 SM   a=dst, b=src, c=len                           spad[a+i] = softmax(int8 spad[b..])[i], Q0.16 (Chapter 6); dst == src or disjoint
  op 6 VADD a=dst, b=src1, c=src2, d[15:0]=len           spad[a+i] = clamp(int8 spad[b+i] + int8 spad[c+i], -127, 127)
  op 7 AMAX a=dst, b=src, c=len                           spad[a] = index of the first maximum of the signed int32 spad[b..b+len-1]
Operand regions of RQ, SM and VADD must be identical or disjoint (the units are pipelined, so partial overlap is undefined and the program generators never produce it).
Instructions execute one at a time, in order."""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from quant import requant
from softmax_gold import softmax_fixed
SPAD, EXT = 4096, 2048
OPS = {"HALT": 0, "LD": 1, "ST": 2, "MM": 3, "RQ": 4, "SM": 5, "VADD": 6, "AMAX": 7}
NAMES = {v: k for k, v in OPS.items()}
FIELDS = {"a": (123, 16), "b": (107, 16), "c": (91, 16), "d": (75, 24), "e": (51, 6), "f": (45, 8), "g": (37, 8), "h": (29, 8), "fl": (21, 4), "x": (17, 18)}
def s8(v): v &= 0xFF; return v - 256 if v & 128 else v
def s32(v): v &= 0xFFFFFFFF; return v - (1 << 32) if v & (1 << 31) else v
def encode(ins):
    """ins: dict with 'op' (name) and field names a,b,c,d,e,f,g,h,fl,x (missing = 0). Returns the 128-bit integer."""
    w = OPS[ins["op"]] << 124
    for name, (hi, width) in FIELDS.items():
        v = ins.get(name, 0); assert 0 <= v < (1 << width), (ins, name); w |= v << (hi - width + 1)
    return w
def decode(w):
    ins = {"op": NAMES[w >> 124]}
    for name, (hi, width) in FIELDS.items(): ins[name] = (w >> (hi - width + 1)) & ((1 << width) - 1)
    return ins
# ---- the assembler: one instruction per line, key=value, in terms of the operation's own names
ARGS = {"LD": ("dst", "src", "len"), "ST": ("src", "dst", "len"), "MM": ("dst", "A", "B", "M", "K", "N", "tb", "lda", "ldb", "ldc"), "RQ": ("dst", "src", "len", "m", "s", "relu"),
        "SM": ("dst", "src", "len"), "VADD": ("dst", "src1", "src2", "len"), "AMAX": ("dst", "src", "len"), "HALT": ()}
def full(ins): return {**{k: 0 for k in FIELDS}, **ins}
def build(op, **k): return full(_build(op, **k))
def _build(op, **k):
    """Build an instruction dict from the operation's own argument names (missing fields are 0)."""
    op = op.upper(); assert set(k) <= set(ARGS[op]), (op, k)
    if op == "LD": return {"op": op, "a": k["dst"], "b": k["src"], "c": k["len"]}
    if op == "ST": return {"op": op, "a": k["src"], "b": k["dst"], "c": k["len"]}
    if op == "MM": return {"op": op, "a": k["dst"], "b": k["A"], "c": k["B"], "f": k["M"], "g": k["K"], "h": k["N"], "fl": k.get("tb", 0), "d": k.get("lda", k["K"]) | (k.get("ldb", 0) << 12), "x": k.get("ldc", k["N"])}
    if op == "RQ": return {"op": op, "a": k["dst"], "b": k["src"], "c": k["len"], "d": k["m"], "e": k["s"], "fl": k.get("relu", 0)}
    if op in ("SM", "AMAX"): return {"op": op, "a": k["dst"], "b": k["src"], "c": k["len"]}
    if op == "VADD": return {"op": op, "a": k["dst"], "b": k["src1"], "c": k["src2"], "d": k["len"]}
    return {"op": "HALT"}
def assemble(text):
    prog = []
    for line in text.splitlines():
        line = line.split(";")[0].strip()
        if not line: continue
        parts = line.split(); kw = {}
        for p in parts[1:]: key, val = p.split("="); kw[key] = int(val, 0)
        prog.append(build(parts[0], **kw))
    return prog
def disassemble(ins):
    op = ins["op"]; a, b, c, d, e, f, g, h, fl, x = (ins[k] for k in "a b c d e f g h fl x".split())
    if op == "LD": return f"LD   dst={a} src={b} len={c}"
    if op == "ST": return f"ST   src={a} dst={b} len={c}"
    if op == "MM": return f"MM   dst={a} A={b} B={c} M={f} K={g} N={h} tb={fl & 1} lda={d & 0xFFF} ldb={d >> 12} ldc={x & 0xFFF}"
    if op == "RQ": return f"RQ   dst={a} src={b} len={c} m={d} s={e} relu={fl & 1}"
    if op == "SM": return f"SM   dst={a} src={b} len={c}"
    if op == "VADD": return f"VADD dst={a} src1={b} src2={c} len={d & 0xFFFF}"
    if op == "AMAX": return f"AMAX dst={a} src={b} len={c}"
    return "HALT"
# ---- the reference simulator
class Machine:
    def __init__(self, ext=None): self.spad = [0] * SPAD; self.ext = list(ext) if ext else [0] * EXT; self.n_inst = 0
    def run(self, prog, maxsteps=100000):
        for ins in prog:
            op = ins["op"]; a, b, c, d, e, f, g, h, fl, x = (ins[k] for k in "a b c d e f g h fl x".split()); sp = self.spad
            if op == "HALT": break
            self.n_inst += 1
            if op == "LD":
                for i in range(c): sp[a + i] = self.ext[b + i] & 0xFFFFFFFF
            elif op == "ST":
                for i in range(c): self.ext[b + i] = sp[a + i] & 0xFFFFFFFF
            elif op == "MM":
                lda, ldb, ldc, tb = d & 0xFFF, d >> 12, x & 0xFFF, fl & 1
                A = [[s8(sp[b + m * lda + k]) for k in range(g)] for m in range(f)]
                B = [[s8(sp[c + (n * ldb + k if tb else k * ldb + n)]) for n in range(h)] for k in range(g)]
                for m in range(f):
                    for n in range(h): sp[a + m * ldc + n] = sum(A[m][k] * B[k][n] for k in range(g)) & 0xFFFFFFFF
            elif op == "RQ":
                r = [requant(s32(sp[b + i]), d, e, bool(fl & 1)) & 0xFFFFFFFF for i in range(c)]
                for i in range(c): sp[a + i] = r[i]
            elif op == "SM":
                p = softmax_fixed([s8(sp[b + i]) for i in range(c)])
                for i in range(c): sp[a + i] = p[i]
            elif op == "VADD":
                n = d & 0xFFFF; r = [max(-127, min(127, s8(sp[b + i]) + s8(sp[c + i]))) & 0xFFFFFFFF for i in range(n)]
                for i in range(n): sp[a + i] = r[i]
            elif op == "AMAX":
                v = [s32(sp[b + i]) for i in range(c)]; sp[a] = v.index(max(v))
        return self
# ---- the cycle model: how many clock cycles each instruction takes, derived from the structure of each unit
# per instruction: 1 fetch + 1 issue + the unit's busy time (WAIT).  The unit times are formulas in the operands; the constants K_* are read off single-instruction runs of the circuit
# (Chapter 7 does this), then FIXED, and checked on random programs to the cycle.
LAT = 8
K = {"LD": 1, "ST": 2, "MM": 7, "RQ": 2, "SM": 6, "VADD": 2, "AMAX": 3}
def wait_cycles(ins):
    op = ins["op"]; a, b, c, d, e, f, g, h, fl, x = (ins[k] for k in "a b c d e f g h fl x".split())
    if op == "LD": return c + LAT + K["LD"]
    if op == "ST": return c + K["ST"]
    if op == "MM": return f * g + g * h + (g + f + h - 2) + f * h + K["MM"]
    if op == "RQ": return c + K["RQ"]
    if op == "SM": return 3 * c + 41 + K["SM"]
    if op == "VADD": return 2 * (d & 0xFFFF) + K["VADD"]
    if op == "AMAX": return c + K["AMAX"]
    return 0
UNIT = {"LD": "dma", "ST": "dma", "MM": "mm", "RQ": "vec", "SM": "vec", "VADD": "vec", "AMAX": "vec"}
def cycle_counts(prog):
    """Predicted [total, mm, dma, vec, n_inst] counters for a whole program ending in HALT (the HALT costs fetch + issue)."""
    cnt = {"mm": 0, "dma": 0, "vec": 0}; total = 0; n = 0
    for ins in prog:
        if ins["op"] == "HALT": break
        w = wait_cycles(ins); cnt[UNIT[ins["op"]]] += w; total += 2 + w; n += 1
    return [total + 2, cnt["mm"], cnt["dma"], cnt["vec"], n]
