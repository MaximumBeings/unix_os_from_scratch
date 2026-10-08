#!/usr/bin/env python3
"""Chapter 11: test the tests of the COMPILER. Break model/compiler.py one line at a time and run the battery: 60 random graphs under checks (a)-(e), the five graphs the compiler must refuse, and a directed cancellation case. 'caught' = some check reports a problem or the compiler crashes."""
import concurrent.futures, os, shutil, subprocess, sys, tempfile
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); SRC = open(os.path.join(ROOT, "model", "compiler.py")).read()
M = [
 ("tiling: edge tiles are always 4 x 4", "mt, nt = min(4, m - i0), min(4, nn - j0)", "mt, nt = 4, 4"),
 ("tiling: the A tile offset ignores the row length", "A=ins[0] + i0 * k,", "A=ins[0] + i0,"),
 ("tiling: the transposed B tile offset ignores the row length", "B=ins[1] + (j0 * k if tb else j0)", "B=ins[1] + j0"),
 ("tiling: the stride of B is the output width even when transposed", "ldb=(k if tb else nn)", "ldb=nn"),
 ("tiling: the transpose flag is dropped", "tb=int(tb), lda=k", "tb=0, lda=k"),
 ("tiling: results are stored with row stride 4", "ldc=nn))", "ldc=4))"),
 ("quantization: the constant factor of a matmul is ignored", 'ms = mantissa_shift(n.attrs["scale"] * scale[n.ins[0]] * scale[n.ins[1]] / scale[n.id])', 'ms = mantissa_shift(scale[n.ins[0]] * scale[n.ins[1]] / scale[n.id])'),
 ("quantization: the second operand's scale is replaced by the first's", 'scale[n.ins[0]] * scale[n.ins[1]] / scale[n.id])', 'scale[n.ins[0]] * scale[n.ins[0]] / scale[n.id])'),
 ("quantization: the fused relu flag is dropped", 'relu=int(P.relu_fused.get(n.id, False)))', 'relu=0)'),
 ("quantization: softmax probabilities are requantized with 1/65536", "ms = mantissa_shift(127 / 65536)", "ms = mantissa_shift(1 / 65536)"),
 ("planning: the softmax input scale is pinned to 1/8", "pinned[find(n.ins[0])] = 1 / 16", "pinned[find(n.ins[0])] = 1 / 8"),
 ("planning: calibration keeps the smallest range, not the largest", "rng[nid] = max(rng.get(nid, 0.0), max(abs(x) for r in m for x in r))", "rng[nid] = min(rng.get(nid, 1e9), max(abs(x) for r in m for x in r))"),
 ("planning: concatenated parts get independent scales", 'if n.op == "concat": union(n.ins[0], n.id); union(n.ins[1], n.id)', 'if n.op == "concat": union(n.ins[0], n.id)'),
 ("planning: the sum's scale ignores the operands' ranges", "            for i in n.ins: grp_max[find(n.id)] = max(grp_max[find(n.id)], rng[i])", "            pass"),
 ("lowering: softmax rows all read the first row", 'B("SM", dst=tmp + r * nn, src=ins[0] + r * nn, len=nn)', 'B("SM", dst=tmp + r * nn, src=ins[0], len=nn)'),
 ("lowering: argmax of every row is written to the first slot", "dst=out + r, src=ins[0] + r * nn, len=nn", "dst=out, src=ins[0] + r * nn, len=nn"),
 ("lowering: an add rescales with the inverse ratio", "ratio = scale[i] / scale[n.id]", "ratio = scale[n.id] / scale[i]"),
 ("lowering: an add never rescales", "if abs(ratio - 1) < 1e-9: srcs.append(ins[k]); continue", "srcs.append(ins[k]); continue"),
 ("lowering: concatenated parts are placed on top of each other", "loc[i] = (b0, off + (0 if k == 0 else N[n.ins[0]].shape[0]))", "loc[i] = (b0, off)"),
 ("lowering: a load copies only the first row", "code.append(B(\"LD\", dst=a, src=e, len=n.shape[0] * n.shape[1]))", "code.append(B(\"LD\", dst=a, src=e, len=n.shape[1]))"),
 ("lowering: an output is stored without its last row", "code.append(B(\"ST\", src=ins[0], dst=e[\"addr\"], len=n.shape[0] * n.shape[1]))", "code.append(B(\"ST\", src=ins[0], dst=e[\"addr\"], len=max(1, n.shape[0] * n.shape[1] - n.shape[1])))"),
 ("lowering: external regions of inputs and weights overlap", "P.ext[n.attrs[\"name\"]] = {\"addr\": ext_top, \"shape\": n.shape, \"node\": n.id, \"kind\": n.op}; ext_top += n.shape[0] * n.shape[1]", "P.ext[n.attrs[\"name\"]] = {\"addr\": ext_top, \"shape\": n.shape, \"node\": n.id, \"kind\": n.op}; ext_top += n.shape[0]"),
 ("allocator: buffers are released one step too early", "if l == n.id and bufs[b][\"addr\"] is not None", "if l <= n.id + 1 and bufs[b][\"addr\"] is not None"),
 ("allocator: free pieces are merged across a gap", "if merged and merged[-1][0] + merged[-1][1] == a:", "if merged and merged[-1][0] + merged[-1][1] <= a:"),
 ("checks: the inner-dimension limit is 640", "if k > 64:", "if k > 640:"),
 ("checks: relu may follow a matmul with other consumers", "or len(cons[src.id]) != 1", ""),
 ("checks: a tensor may feed two concatenations", 'if i in loc: raise CompileError(f"node {i} feeds two concatenations")', "pass"),
]
EQUIV = [("EQUIVALENT in effect: the allocator skips an exact-fit hole (it uses the next larger one, or fails only when memory is nearly full)", "if sz >= n:", "if sz > n:")]
def check(label, old, new):
    if SRC.count(old) != 1: return label, "BAD ANCHOR (%d occurrences)" % SRC.count(old)
    d = tempfile.mkdtemp(prefix="mut11_"); shutil.copytree(os.path.join(ROOT, "model"), os.path.join(d, "model")); open(os.path.join(d, "model", "compiler.py"), "w").write(SRC.replace(old, new))
    code = "import sys; sys.path.insert(0,'model'); import compiler_tests as T; p,w=T.check_many(range(60)); e=T.check_errors(); c=T.check_cancellation(); sys.exit(1 if (p or e or c) else 0)"
    try: r = subprocess.run([sys.executable, "-c", code], cwd=d, capture_output=True, text=True, timeout=900).returncode != 0
    except subprocess.TimeoutExpired: r = True
    shutil.rmtree(d, ignore_errors=True); return label, r
print("NOTE: this script deliberately breaks copies of the compiler. 'caught' is EXPECTED: it shows the battery notices the mistake.")
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool: rows = list(pool.map(lambda m: check(*m), M)); eq = list(pool.map(lambda m: check(*m), EQUIV))
n = 0
for label, r in rows: print(f"{label}: {'caught' if r is True else r if isinstance(r, str) else 'NOT CAUGHT'}"); n += r is True
print(f"\ncompiler: {n} of {len(M)} broken compilers caught")
print("\nA change that looks like a bug and is not:")
for label, r in eq: print(f"{label}: {'caught' if r is True else 'NOT CAUGHT'}")
sys.exit(0 if n == len(M) else 1)
