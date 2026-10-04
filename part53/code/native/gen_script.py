#!/usr/bin/env python3
"""Chapter 53: random scripts for the differential test. gen(seed, n) -> text. Keys: short text, 24-byte keys, binary keys (x:HEX) with 00 and FF bytes, one-byte keys; values: empty, short, 100 bytes. Operations: put (most), delete, get, flush, compact, reopen, digest, stats, and every
~40 operations a CRASH: `crash N` with a random budget, more commands until the file system is dead, then `recover`, `digest` and `dump`."""
import random, sys
def gen(seed, n=200):
    R = random.Random(seed); keys = []
    for i in range(R.choice([8, 20, 60, 120])):
        t = R.random()
        if t < .5: keys.append("k%d" % i)
        elif t < .65: keys.append("key-" + "x" * R.randrange(0, 20))
        elif t < .8: keys.append("x:" + bytes([R.choice([0, 255, R.randrange(256)]) for _ in range(R.randrange(1, 25))]).hex())
        elif t < .9: keys.append("x:%02x" % R.randrange(256))
        else: keys.append("long" + "L" * 20)
    def val():
        t = R.random()
        if t < .1: return "-"
        if t < .2: return "x:" + bytes(R.randrange(256) for _ in range(100)).hex()
        return "v%d_" % R.randrange(10**6) + "y" * R.randrange(0, 40)
    out = []; since = 0
    for i in range(n):
        c = R.random()
        if c < .55: out.append(f"put {R.choice(keys)} {val()}")
        elif c < .70: out.append(f"del {R.choice(keys)}")
        elif c < .85: out.append(f"get {R.choice(keys)}")
        elif c < .88: out.append("flush")
        elif c < .90: out.append("compact")
        elif c < .92: out.append("open")
        elif c < .96: out.append("digest")
        else: out.append("stats")
        since += 1
        if since >= 40 and R.random() < .3:
            out.append("crash %d" % R.choice([0, 1, R.randrange(2, 300), R.randrange(300, 3000), R.randrange(3000, 9000)]))
            for j in range(R.randrange(10, 60)): out.append(R.choice([f"put {R.choice(keys)} {val()}", f"del {R.choice(keys)}", "flush", "compact"]))
            out += ["recover", "digest", "dump"]; since = 0
    out += ["digest", "stats", "dump"]; return "\n".join(out) + "\n"
if __name__ == "__main__": sys.stdout.write(gen(int(sys.argv[1]), int(sys.argv[2]) if len(sys.argv) > 2 else 200))
