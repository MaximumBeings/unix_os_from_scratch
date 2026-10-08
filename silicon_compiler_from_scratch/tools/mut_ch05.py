#!/usr/bin/env python3
"""Chapter 5: test the tests. Break the DMA, the scratchpad controller and the double-buffering rules one line at a time; the testbench runs two shapes (one load-bound, one compute-bound). A mutant counts as caught if either shape catches it."""
import os, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import hw
D = "rtl/dbuf.v"; A = "rtl/dma.v"; S = "rtl/sram.v"
M = [
 (D, "controller: a load may overwrite a bank that is still full", "!ld_active && !d_busy && ld_n < ntiles && !full[nb] &&", "!ld_active && !d_busy && ld_n < ntiles &&"),
 (D, "controller: double buffering is never used (always serial)", "(dbl || (full == 2'b00 && !c_busy))", "(full == 2'b00 && !c_busy)"),
 (D, "controller: serial mode overlaps like double buffering", "(dbl || (full == 2'b00 && !c_busy))", "1'b1"),
 (D, "controller: every load goes to bank 0", "d_dst <= (nb ? TILE : 0);", "d_dst <= 0;"),
 (D, "controller: compute always reads bank 0", "assign raddr = (c_bank ? TILE : 0) + wi;", "assign raddr = wi;"),
 (D, "controller: the base address is ignored", "d_src <= base + ld_n * TILE;", "d_src <= ld_n * TILE;"),
 (D, "controller: tile addresses overlap by one word", "d_src <= base + ld_n * TILE;", "d_src <= base + ld_n * (TILE - 1);"),
 (D, "controller: the DMA is told to move one word too few", ".len(TILE[AW-1:0])", ".len(TILE[AW-1:0] - 1)"),
 (D, "controller: the accumulator is not cleared between tiles", "drain <= 0; acc <= 0; end", "drain <= 0; end"),
 (D, "controller: the last word is never added", "if (rd_v) acc <= acc + rdata;", "if (rd_v && !drain) acc <= acc + rdata;"),
 (D, "controller: the sum is reported before the last read arrives", "wire c_finish = c_busy && drain && !rd_v;", "wire c_finish = c_busy && drain;"),
 (D, "controller: the last word of the tile is skipped", "if (wi == TILE - 1) drain <= 1;", "if (wi == TILE - 2) drain <= 1;"),
 (D, "controller: one tile too many is loaded", "ld_n < ntiles &&", "ld_n <= ntiles &&"),
 (D, "controller: a word is read in the first cycle of its slot (timing changes)", "ph == CPW - 1;", "ph == 0;"),
 (D, "controller: compute waits on the wrong bank's flag", "full[cp_n[0]] && cp_n < ntiles", "full[0] && cp_n < ntiles"),
 (A, "dma: requests all go to the first word", "assign req_addr = src_r + issued;", "assign req_addr = src_r;"),
 (A, "dma: answers are written one word too high", "assign sram_waddr = dst_r + recvd;", "assign sram_waddr = dst_r + recvd + 1;"),
 (A, "dma: done is raised one answer too early", "if (recvd == len_r - 1) begin", "if (recvd == len_r - 2) begin"),
 (A, "dma: writes only every other answer", "assign sram_we = resp_valid;", "assign sram_we = resp_valid && !recvd[0];"),
 (S, "sram: the write is lost", "if (we) mem[waddr] <= wdata;", "if (we && waddr == 0) mem[waddr] <= wdata;"),
]
F = ["rtl/sram.v", "rtl/dma.v", "rtl/dbuf.v", "tb/extmem.v"]; SHAPES = [(16, 1, 4, 6), (8, 4, 3, 5)]
print("NOTE: this script deliberately breaks copies of the RTL. 'caught' lines are EXPECTED: they show the testbench can detect the mistake.")
res = {m[1]: [] for m in M}
for tile, cpw, lat, T in SHAPES:
    subprocess.run([sys.executable, "model/dbuf_gold.py", "out/dbuf_vectors.hex", str(tile), str(cpw), str(lat), str(T)], cwd=hw.ROOT, capture_output=True)
    c, n, lines = hw.mutate(M, F, "dbuf_tb", ["tb/dbuf_tb.v"], defines=(f"TILE={tile}", f"CPW={cpw}", f"LAT={lat}"))
    for line in lines: label, status = line.rsplit(": ", 1); res[label].append(status)
caught = 0
for label, st in res.items():
    ok = "caught" in st; caught += ok
    print(f"{label}: {'caught' if ok else 'NOT CAUGHT'}   [{SHAPES[0][0]}w/cpw{SHAPES[0][1]}: {st[0]}; {SHAPES[1][0]}w/cpw{SHAPES[1][1]}: {st[1]}]")
print(f"\ntile streamer: {caught} of {len(M)} broken circuits caught")
sys.exit(0 if caught == len(M) else 1)
