#!/usr/bin/env python3
"""Chapter 9: test the tests of the batched program builder (model/batch.py). The check is BATCH INVARIANCE: each sequence's output and cache rows must equal what the same sequence gives when decoded alone, for batches of 1 to 4 and for a ragged batch;
a batch of one must also equal the integer reference. Each mutant breaks one line of the builder; 'caught' means the check reports a problem or crashes."""
import concurrent.futures, os, shutil, subprocess, sys, tempfile
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); SRC = open(os.path.join(ROOT, "model", "batch.py")).read()
M = [
 ("projection runs on one row (M = 1) instead of the whole batch", "A=SP_X, B=SP_W + 256 * wi + 4 * c, M=n,", "A=SP_X, B=SP_W + 256 * wi + 4 * c, M=1,"),
 ("projection results are stored with row stride 4, not d", "tb=0, lda=DM, ldb=D, ldc=D))", "tb=0, lda=DM, ldb=D, ldc=4))"),
 ("every new key is taken from row 0 of the projection", "src=SP_KA + j * D, len=D", "src=SP_KA, len=D"),
 ("every new value is taken from row 0 of the projection", "src=SP_VA + j * D, len=D", "src=SP_VA, len=D"),
 ("every sequence's cache lives in batch slot 0", "def SP_KC(j): return 1280 + 512 * j", "def SP_KC(j): return 1280"),
 ("the value caches of all sequences overlap", "def SP_VC(j): return 1280 + 512 * j + 256", "def SP_VC(j): return 1280 + 256 * j + 256"),
 ("sequence j attends with the query of row 0", "A=SP_Q8 + j * D, B=SP_KC(j)", "A=SP_Q8, B=SP_KC(j)"),
 ("hidden states are all loaded into slot 0", "dst=SP_X + j * DM, src=EX_X(b)", "dst=SP_X, src=EX_X(b)"),
 ("every sequence reads the hidden state of sequence 0", "src=EX_X(b) + (ts[j] - 1) * DM", "src=EX_X(0) + (ts[j] - 1) * DM"),
 ("outputs are all stored in sequence 0's output slot", "B_(\"ST\", src=SP_O8, dst=EX_OUT(b), len=D)", "B_(\"ST\", src=SP_O8, dst=EX_OUT(0), len=D)"),
 ("the new key is appended at the first sequence's position", "r = (ts[j] - 1) * D", "r = (ts[0] - 1) * D"),
 ("the value cache is loaded from the key cache", "B_(\"LD\", dst=SP_VC(j), src=EX_VC(b)", "B_(\"LD\", dst=SP_VC(j), src=EX_KC(b)"),
 ("the new value is appended to the key cache in external memory", "B_(\"ST\", src=SP_VC(j) + r, dst=EX_VC(b) + r", "B_(\"ST\", src=SP_VC(j) + r, dst=EX_KC(b) + r"),
 ("attention length of every sequence is the first sequence's", "L = ts[j]\n", "L = ts[0]\n"),
 ("the q requantization covers only one row", "src=SP_QA, len=n * D", "src=SP_QA, len=D"),
 ("a sequence's cache is loaded with the first sequence's length", "len=(ts[j] - 1) * D), B_(\"LD\", dst=SP_VC(j), src=EX_VC(b), len=(ts[j] - 1) * D)", "len=(ts[0] - 1) * D), B_(\"LD\", dst=SP_VC(j), src=EX_VC(b), len=(ts[j] - 1) * D)"),
]
def check(label, old, new, ragged=True):
    if SRC.count(old) != 1: return label, "BAD ANCHOR (%d occurrences)" % SRC.count(old)
    d = tempfile.mkdtemp(prefix="mut9_"); shutil.copytree(os.path.join(ROOT, "model"), os.path.join(d, "model")); open(os.path.join(d, "model", "batch.py"), "w").write(SRC.replace(old, new))
    r = subprocess.run([sys.executable, "-c", "import sys; sys.path.insert(0,'model'); import batch; p=batch.check_batching(ragged=%s); sys.exit(1 if p else 0)" % ragged], cwd=d, capture_output=True, text=True, timeout=600); shutil.rmtree(d, ignore_errors=True)
    return label, r.returncode != 0
print("NOTE: this script deliberately breaks copies of the batched program builder. 'caught' is EXPECTED: it shows batch invariance notices the mistake.")
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    rows = list(pool.map(lambda m: check(*m), M)); rows2 = list(pool.map(lambda m: check(m[0], m[1], m[2], False), M))
n = 0; n2 = 0
print(f"{'':75s} {'full check':>11s} {'no ragged batch':>16s}")
for (label, r), (_, r2) in zip(rows, rows2):
    print(f"{label:75s} {'caught' if r is True else 'NOT CAUGHT':>11s} {'caught' if r2 is True else 'NOT CAUGHT':>16s}"); n += r is True; n2 += r2 is True
print(f"\nbatched builder: {n} of {len(M)} broken programs caught by the full check; {n2} of {len(M)} if the ragged batch (different lengths in one step) is left out")
sys.exit(0 if n == len(M) else 1)
