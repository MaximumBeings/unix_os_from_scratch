#!/usr/bin/env python3
"""Chapter 5: the answer key for the tile streamer: the sum (mod 2^32) of every tile, and a SCHEDULE MODEL that predicts how many cycles the stream takes, serial and double-buffered.
  load time of one tile    L = TILE + LAT + L0          (issue one word per cycle, the last answer arrives LAT cycles later, L0 cycles of handshake)
  compute time of a tile   C = TILE * CPW + C0
  serial:           T * (L + C)                                  + S0
  double-buffered:  L + (T - 1) * max(L, C) + C                  + D0      (the load of tile n+1 hides behind the compute of tile n; the slower of the two sets the pace)
The FORM of these formulas is derived from the schedule; the four small constants L0, C0, S0, D0 are the handshake overheads of this particular controller. They were read off ONE configuration of the simulated circuit and are fixed here; the book then checks them
on seven other configurations (different TILE, CPW, LAT, tile count) to the cycle. Usage: dbuf_gold.py OUTFILE TILE CPW LAT NTILES"""
import random, sys
L0, C0, S0, D0 = 3, 3, 0, 0
def cycles(T, tile, cpw, lat):
    L = tile + lat + L0; C = tile * cpw + C0
    return T * (L + C) + S0, L + (T - 1) * max(L, C) + C + D0
if __name__ == "__main__":
    out, tile, cpw, lat, T = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5])
    R = random.Random(tile * 1000 + cpw * 100 + lat * 10 + T); data = [R.randrange(0, 2**32) for _ in range(T * tile)]
    data[0:tile] = [0xFFFFFFFF] * tile                       # tile 0: the sum wraps around 2^32
    sums = [sum(data[n * tile:(n + 1) * tile]) & 0xFFFFFFFF for n in range(T)]
    cs, cd = cycles(T, tile, cpw, lat)
    words = [T, tile, cs, cd] + data + sums
    open(out, "w").write("\n".join("%08x" % w for w in words) + "\n"); print(f"{T} tiles of {tile} words written to {out}; schedule model: serial {cs} cycles, double-buffered {cd}")
