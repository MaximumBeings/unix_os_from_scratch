#!/usr/bin/env python3
"""Chapter 51: differential test. N random order flows (gen_flow.py) run through the C engine (book_cli, the SAME 051_book.c the kernel links, with AddressSanitizer + UBSan) and the independent Python engine (book_ref.py): the whole text output --
every verdict, every trade, every feed message in hex, the final book, its hash, and the verdict on the book REBUILT from the feed -- must be identical. Then 3 damaged versions of each flow's feed (a flipped byte, a cut, a deleted message, a duplicated message, a swapped pair)
go to both subscribers: both must accept or refuse identically, with the same reason and message number, and the same book hash if accepted. Usage: diff_book.py N [cli]"""
import collections, os, random, subprocess, sys, tempfile
here = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, here); sys.path.insert(0, os.path.join(here, ".."))
import gen_flow
N = int(sys.argv[1]); cli = sys.argv[2] if len(sys.argv) > 2 else "/tmp/kcli"; tmp = tempfile.mkdtemp(); stats = collections.Counter(); diffs = 0; ref = os.path.join(here, "..", "book_ref.py")
def py(mode, p): return subprocess.run([sys.executable, ref, mode, p], capture_output=True, text=True).stdout
def split(feed):
    out = []; i = 0
    while i + 2 <= len(feed): l = int.from_bytes(feed[i:i + 2], "big"); out.append(feed[i:i + 2 + l]); i += 2 + l
    return out
for seed in range(N):
    if diffs and os.environ.get("STOP_AT_FIRST"): break
    p = os.path.join(tmp, "f.txt"); n = [60, 300, 700][seed % 3]; open(p, "w").write(gen_flow.gen(seed, n)); c = subprocess.run([cli, "run", p], capture_output=True, text=True)
    r = py("run", p)
    if c.returncode != 0 or c.stderr: print("CRASH", seed, c.stderr[:200]); diffs += 1; continue
    if c.stdout != r: diffs += 1; print("RUN DIFFERENT seed", seed); print(c.stdout[-600:], "--- python\n", r[-600:]) if diffs <= 2 else None; continue
    stats["order flows"] += 1; stats["commands"] += r.count("\ncmd ") + 1; stats["trades"] += r.count("\ntrade "); stats["feed messages"] += sum(len(split(bytes.fromhex(l[5:]))) for l in r.splitlines() if l.startswith("feed "))
    for l in r.splitlines():
        if l.startswith("cmd ") and not l.endswith(" ok"): stats["refused: " + l.split()[2]] += 1
    feed = b"".join(bytes.fromhex(l[5:]) for l in r.splitlines() if l.startswith("feed ")); msgs = split(feed); R = random.Random(seed)
    for v in range(3):
        m = list(msgs); k = R.choice(["flip", "cut", "del", "dup", "swap"]); b = bytearray(b"".join(m))
        if k == "flip": b[R.randrange(len(b))] ^= 1 << R.randrange(8); b = bytes(b)
        elif k == "cut": b = bytes(b[:R.randrange(1, len(b))])
        elif k == "del": del m[R.randrange(len(m))]; b = b"".join(m)
        elif k == "dup": i = R.randrange(len(m)); m.insert(i, m[i]); b = b"".join(m)
        else:
            i = R.randrange(len(m) - 1); m[i], m[i + 1] = m[i + 1], m[i]; b = b"".join(m)
        q = os.path.join(tmp, "d.feed"); open(q, "wb").write(b); cc = subprocess.run([cli, "feed", q], capture_output=True, text=True); pp = py("feed", q)
        if cc.stdout != pp: diffs += 1; print("FEED DIFFERENT seed", seed, k); print(cc.stdout[:300], "---\n", pp[:300]) if diffs <= 3 else None; break
        stats["damaged feeds compared (" + k + ")"] += 1; stats["... refused" if cc.stdout.startswith("REFUSED") else "... still accepted (a consistent book)"] += 1
for k in sorted(stats): print(f"  {k}: {stats[k]}")
print(f"{N} order flows, {diffs} differences"); sys.exit(1 if diffs else 0)
