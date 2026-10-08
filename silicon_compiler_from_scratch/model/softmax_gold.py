#!/usr/bin/env python3
"""Chapter 6: the answer keys for the exp table, the divider and the fixed-point softmax, in plain Python integers (the table with math.exp, the circuit's table was made with `decimal`).
  softmax_gold.py table  OUTFILE                 256 expected table entries
  softmax_gold.py div    OUTFILE [COUNT]          divider cases:  [ncases] then {num, den, quot, rem} as 4 x 10 hex digits per line (W = 40)
  softmax_gold.py soft   OUTFILE N [COUNT]        softmax cases:  [ncases, N] then per case N input words (sign-extended int8) and N output words (Q0.16)
THE SOFTMAX ALGORITHM (integers only):  x_i are int8 scores, real value x_i/16.   m = max x;   e_i = T[m - x_i] with T[d] = round(65535 exp(-d/16));   s = sum e_i;   r = floor(2^38 / s);   p_i = (e_i * r + 2^21) >> 22   (Q0.16, 65536 = 1.0)."""
import math, random, sys
def table(): return [int(math.floor(65535 * math.exp(-d / 16) + 0.5)) for d in range(256)]
T = table()
def divu(num, den, W=40):
    if den == 0: return (1 << W) - 1, num                         # defined behaviour of the hardware divider
    return num // den, num % den
def softmax_fixed(xs):
    m = max(xs); e = [T[m - x] for x in xs]; s = sum(e); r = (1 << 38) // s
    return [(ei * r + (1 << 21)) >> 22 for ei in e]
def softmax_float(xs):
    m = max(xs); e = [math.exp((x - m) / 16) for x in xs]; s = sum(e); return [v / s for v in e]
def divide_cases(count):
    R = random.Random(6); W = 40; mask = (1 << W) - 1; rows = []
    for num, den in [(0, 1), (1, 1), (mask, 1), (mask, mask), (mask, mask - 1), (5, 7), (7, 5), (1 << 38, 65535), (1 << 38, 1 << 22), (1 << 38, 3), (0, 0), (123456789, 0), (mask, 0), (mask - 1, mask), (1 << 39, (1 << 39) + 1)]:
        rows.append((num, den))
    for _ in range(count):
        nb, db = R.randrange(1, W + 1), R.randrange(1, W + 1); rows.append((R.getrandbits(nb), R.getrandbits(db)))
    for s in range(65535, 65535 * 64, 997): rows.append((1 << 38, s))        # the denominators softmax actually uses
    return rows
def softmax_cases(N, count):
    R = random.Random(60 + N); xs = [[0] * N, [-128] * N, [127] * N, [127] + [-128] * (N - 1), [-128] * (N - 1) + [127], list(range(-128, -128 + N)), [(-1) ** i * 100 for i in range(N)]]
    xs += [[R.randrange(-128, 128) for _ in range(N)] for _ in range(count)]
    xs += [[R.randrange(-8, 8) for _ in range(N)] for _ in range(count // 4)]                                                    # narrow ranges: ties and small differences
    xs += [[R.choice([R.randrange(-128, 128), 127, -128]) for _ in range(N)] for _ in range(count // 4)]
    return xs
if __name__ == "__main__":
    kind, out = sys.argv[1], sys.argv[2]
    if kind == "table": open(out, "w").write("\n".join("%04x" % v for v in T) + "\n"); print("256 table entries written to", out)
    elif kind == "div":
        rows = divide_cases(int(sys.argv[3]) if len(sys.argv) > 3 else 20000); lines = ["%010x" % len(rows)]
        for n, d in rows: q, r = divu(n, d); lines.append("%010x%010x%010x%010x" % (n, d, q, r))
        open(out, "w").write("\n".join(lines) + "\n"); print(len(rows), "division cases written to", out)
    elif kind == "soft":
        N = int(sys.argv[3]); cs = softmax_cases(N, int(sys.argv[4]) if len(sys.argv) > 4 else 2000); w = lambda v: "%08x" % (v & 0xFFFFFFFF); words = [len(cs), N]
        for xs in cs: words += [w(x) for x in xs] + [w(p) for p in softmax_fixed(xs)]
        open(out, "w").write("\n".join(w(x) if isinstance(x, int) else x for x in words) + "\n"); print(len(cs), f"softmax cases (N={N}) written to", out)
