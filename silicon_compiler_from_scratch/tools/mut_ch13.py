#!/usr/bin/env python3
"""Chapter 13: test the tests of the UNPACK unit and its wiring. Each mutant breaks one line; the suite is model/unpack_progs.py (directed + 40 random programs). Equivalent mutants are listed separately."""
import os, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import hw
V = "rtl/ga2_vec.v"; T = "rtl/ga2.v"
MUT = [
 (V, "UNPACK: the fields are taken in reverse order", "wire [3:0] nib = w[4*j +: 4];", "wire [3:0] nib = w[4*(7-j) +: 4];"),
 (V, "UNPACK: the fields are not sign-extended", "assign wd = {{28{nib[3]}}, nib};", "assign wd = {28'd0, nib};"),
 (V, "UNPACK: outputs advance 4 words per source word, not 8", "assign wa = dst + {i, 3'b000} + j;", "assign wa = dst + {1'b0, i, 2'b00} + j;"),
 (V, "UNPACK: one source word too many", "if (i == len - 1) begin busy <= 1'b0; done <= 1'b1; end else begin i <= i + 1; st <= S_RD; end", "if (i == len) begin busy <= 1'b0; done <= 1'b1; end else begin i <= i + 1; st <= S_RD; end"),
 (V, "UNPACK: the last field of each word is skipped", "if (j == 7) begin", "if (j == 6) begin"),
 (V, "UNPACK: the word is latched before the read data is valid", "S_RD: st <= S_LAT;", "S_RD: begin st <= S_WR; w <= rd; j <= 0; end"),
 (V, "UNPACK: reads one word too high", "assign ra = src + i; assign we = busy && st == S_WR;", "assign ra = src + i + 1; assign we = busy && st == S_WR;"),
 (V, "UNPACK: the length field is 8 bits wide", "len <= ins[87:76]; i <= 0; j <= 0; st <= S_RD;", "len <= {4'd0, ins[83:76]}; i <= 0; j <= 0; st <= S_RD;"),
 (V, "UNPACK: the destination field is read from the source field", "dst <= ins[119:108]; src <= ins[103:92]; len <= ins[87:76]; i <= 0; j <= 0; st <= S_RD;", "dst <= ins[103:92]; src <= ins[103:92]; len <= ins[87:76]; i <= 0; j <= 0; st <= S_RD;"),
 (T, "sequencer: UNPACK's write enable is dropped", "(cur == OP_UNPACK) ? un_we : 1'b0;", "1'b0;"),
 (T, "sequencer: UNPACK is counted as DMA time", "else if (cur == OP_LD || cur == OP_ST) cyc_dma", "else if (cur == OP_LD || cur == OP_ST || cur == OP_UNPACK) cyc_dma"),
 (T, "sequencer: UNPACK's completion is never seen", "|| (cur == OP_UNPACK && un_done);", ";"),
]
F = ["rtl/sram.v", "rtl/dma.v", "rtl/systolic.v", "rtl/requant.v", "rtl/exp_lut.v", "rtl/divu.v", "rtl/rowmem.v", "rtl/ga2_vec.v", "rtl/ga2_sm.v", "rtl/ga2_mm.v", "rtl/ga2.v"]; TB = ["tb/extmem_rw.v", "tb/ga2_tb.v"]
subprocess.run([sys.executable, "model/unpack_progs.py", "out/ga2_programs.hex", "40", "1"], cwd=hw.ROOT, capture_output=True)
print("NOTE: this script deliberately breaks copies of the RTL. 'caught' lines are EXPECTED: they show the programs notice the mistake.")
c, n, l = hw.mutate(MUT, F, "ga2_tb", TB, workers=6); print("\n".join(l)); print(f"\nUNPACK: {c} of {n} broken circuits caught"); sys.exit(0 if c == n else 1)
