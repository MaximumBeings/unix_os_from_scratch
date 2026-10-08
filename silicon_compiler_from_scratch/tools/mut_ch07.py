#!/usr/bin/env python3
"""Chapter 7: test the tests. Break GA-2 one line at a time (sequencer, store, requantizer, vector add, argmax, softmax, matrix unit) and run the 78-program suite. Equivalent mutants are listed separately."""
import os, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import hw
T = "rtl/ga2.v"; V = "rtl/ga2_vec.v"; S = "rtl/ga2_sm.v"; M = "rtl/ga2_mm.v"
MUT = [
 (T, "sequencer: the program counter skips an instruction", "pc <= pc + 1; n_inst <= n_inst + 1;", "pc <= pc + 2; n_inst <= n_inst + 1;"),
 (T, "sequencer: the instruction counter adds two", "n_inst <= n_inst + 1; st <= FETCH;", "n_inst <= n_inst + 2; st <= FETCH;"),
 (T, "sequencer: stores are counted as vector time, not DMA time", "else if (cur == OP_LD || cur == OP_ST) cyc_dma", "else if (cur == OP_LD) cyc_dma"),
 (T, "sequencer: the fetch cycle is not counted", "FETCH: begin ins <= imem_data; st <= ISSUE; cyc_total <= cyc_total + 1; end", "FETCH: begin ins <= imem_data; st <= ISSUE; end"),
 (T, "sequencer: the VADD and AMAX write-data ports are swapped", "(cur == OP_VADD) ? va_wd : (cur == OP_UNPACK) ? un_wd : am_wd;", "(cur == OP_VADD) ? am_wd : (cur == OP_UNPACK) ? un_wd : va_wd;"),
 (T, "sequencer: RQ reads through the softmax unit's address", "(cur == OP_RQ) ? rq_ra : (cur == OP_SM) ? sm_ra", "(cur == OP_RQ) ? sm_ra : (cur == OP_SM) ? sm_ra"),
 (T, "sequencer: the matrix unit's write enable is dropped", "(cur == OP_MM) ? mm_we : (cur == OP_RQ) ? rq_we", "(cur == OP_MM) ? 1'b0 : (cur == OP_RQ) ? rq_we"),
 (T, "sequencer: the load length field is 8 bits wide", ".len(ins[87:76])", ".len({4'd0, ins[83:76]})"),
 (V, "ST: the external address follows the read index", "assign wr_addr = dst + j;", "assign wr_addr = dst + i;"),
 (V, "ST: reads one word too high", "assign ra = src + i; assign wr_valid", "assign ra = src + i + 1; assign wr_valid"),
 (V, "ST: the destination is taken from the source field", "src <= ins[119:108]; dst <= ins[107:92];", "src <= ins[119:108]; dst <= ins[119:108];"),
 (V, "RQ: results are zero-extended, not sign-extended", "assign wd = {{24{q[7]}}, q};", "assign wd = {24'd0, q};"),
 (V, "RQ: the relu flag is read from the wrong bit", "relu <= ins[18];", "relu <= ins[19];"),
 (V, "RQ: the shift field is off by one bit", "s <= ins[51:46];", "s <= ins[52:47];"),
 (V, "RQ: the mantissa loses its top bit", "m <= ins[75:52];", "m <= {1'b0, ins[74:52]};"),
 (V, "RQ: the write address follows the read index", "assign we = v; assign wa = dst + j;", "assign we = v; assign wa = dst + i;"),
 (V, "VADD: the lower clamp is -128", "(sum < -9'sd127) ? -8'sd127 : sum[7:0];", "(sum < -9'sd127) ? -8'sd128 : sum[7:0];"),
 (V, "VADD: the upper clamp is 126", "(sum > 9'sd127) ? 8'sd127", "(sum > 9'sd127) ? 8'sd126"),
 (V, "VADD: no clamp at all", "wire signed [7:0] r = (sum > 9'sd127) ? 8'sd127 : (sum < -9'sd127) ? -8'sd127 : sum[7:0];", "wire signed [7:0] r = sum[7:0];"),
 (V, "VADD: writes on the wrong half of the cycle pair", "assign we = v && par;", "assign we = v && !par;"),
 (V, "VADD: the length field is 8 bits wide", "len <= ins[67:52];", "len <= {8'd0, ins[59:52]};"),
 (V, "AMAX: returns the last maximum, not the first", "$signed(rd) > best", "$signed(rd) >= best"),
 (V, "AMAX: compares unsigned", "$signed(rd) > best", "rd > best"),
 (V, "AMAX: writes the maximum value instead of its index", "assign wd = {20'd0, bidx};", "assign wd = best;"),
 (S, "SM: the maximum starts at 0, not -128", "m <= -8'sd128; sum <= 0; st <= P1;", "m <= 8'sd0; sum <= 0; st <= P1;"),
 (S, "SM: the sum is not cleared between rows", "m <= -8'sd128; sum <= 0; st <= P1;", "m <= -8'sd128; st <= P1;"),
 (S, "SM: no rounding constant", "56'd2097152;", "56'd0;"),
 (S, "SM: the output slice is one bit low", "{15'd0, rounded[38:22]}", "{15'd0, rounded[37:21]}"),
 (S, "SM: the stored exponential keeps only 8 bits", "{16'd0, e}", "{24'd0, e[7:0]}"),
 (S, "SM: pass 3 reads the source, not the stored exponentials", "assign ra = (st == P3) ? dst + i : src + i;", "assign ra = src + i;"),
 (S, "SM: the reciprocal numerator is 2^37", ".num(40'd1 << 38)", ".num(40'd1 << 37)"),
 (S, "SM: the maximum is found with an unsigned compare", "if (st == P1 && x > m) m <= x;", "if (st == P1 && $unsigned(x) > $unsigned(m)) m <= x;"),
 (S, "SM: the output is written in every pass", "assign we = v && (st == P2 || st == P3);", "assign we = v;"),
 (S, "SM: destination and source fields are swapped", "dst <= ins[119:108]; src <= ins[103:92]; len <= ins[87:76]; i <= 0; j <= 0; v <= 1'b0; m <=", "src <= ins[119:108]; dst <= ins[103:92]; len <= ins[87:76]; i <= 0; j <= 0; v <= 1'b0; m <="),
 (M, "MM: transposed B ignores tb in the inner step", "wire [11:0] stepn = tb ? ldb : 12'd1;", "wire [11:0] stepn = 12'd1;"),
 (M, "MM: the k step of B uses ldb even when transposed", "rowbase <= rowbase + (tb ? 12'd1 : ldb); ptr <= rowbase + (tb ? 12'd1 : ldb);", "rowbase <= rowbase + ldb; ptr <= rowbase + ldb;"),
 (M, "MM: B's row step uses N instead of ldb", "rowbase <= rowbase + (tb ? 12'd1 : ldb); ptr <= rowbase + (tb ? 12'd1 : ldb);", "rowbase <= rowbase + (tb ? 12'd1 : N); ptr <= rowbase + (tb ? 12'd1 : N);"),
 (M, "MM: A's row step uses K instead of lda", "rowbase <= rowbase + lda; ptr <= rowbase + lda;", "rowbase <= rowbase + K; ptr <= rowbase + K;"),
 (M, "MM: C's row step uses N instead of ldc", "orow <= orow + ldc; oaddr <= orow + ldc;", "orow <= orow + N; oaddr <= orow + N;"),
 (M, "MM: the stream is one cycle short", "if (t == K + M + N - 3) begin st <= OUT;", "if (t == K + M + N - 4) begin st <= OUT;"),
 (M, "MM: the stream is one cycle long", "if (t == K + M + N - 3) begin st <= OUT;", "if (t == K + M + N - 2) begin st <= OUT;"),
 (M, "MM: the array is not cleared before a product", "wire clr_s = (st == PRE);", "wire clr_s = 1'b0;"),
 (M, "MM: the result is stored transposed", "assign wd = cbus[32*(m*4 + n) +: 32];", "assign wd = cbus[32*(n*4 + m) +: 32];"),
 (M, "MM: K is a 6-bit field", "K <= ins[36:30];", "K <= {1'b0, ins[35:30]};"),
 (M, "MM: M is a 2-bit field", "M <= ins[41:38];", "M <= {2'b0, ins[39:38]};"),
 (M, "MM: B's column memories are written in reverse", ".we(v && st == LB && n_d == gi)", ".we(v && st == LB && n_d == (3 - gi))"),
 (M, "MM: A's row memories are written in reverse", ".we(v && st == LA && m_d == gi)", ".we(v && st == LA && m_d == (3 - gi))"),
 (M, "MM: the b edge takes its values from the A memories", "bedge[8*ii +: 8] = b_rd[ii];", "bedge[8*ii +: 8] = a_rd[ii];"),
 (M, "MM: B is fed one cycle late", "a_ra[ii] = t[5:0] - ii[5:0]; b_ra[ii] = t[5:0] - ii[5:0];", "a_ra[ii] = t[5:0] - ii[5:0]; b_ra[ii] = t[5:0] - ii[5:0] + 6'd1;"),
]
EQUIV = [
 (M, "EQUIVALENT within the ISA: M is a 3-bit field (every legal M, 1 to 4, fits in three bits)", "M <= ins[41:38];", "M <= {1'b0, ins[40:38]};"),
 (T, "EQUIVALENT within the tested range: the load source address is 12 bits wide (the test memory has 2048 words)", ".src(ins[107:92])", ".src({4'd0, ins[103:92]})"),
 (V, "EQUIVALENT: VADD reads its two sources in the other order (addition commutes)", "assign ra = cnt[0] ? (s2 + cnt[16:1]) : (s1 + cnt[16:1]);", "assign ra = cnt[0] ? (s1 + cnt[16:1]) : (s2 + cnt[16:1]);"),
 (M, "EQUIVALENT: rows of A beyond M are not masked off (PE(i,j) depends only on row i and column j)", "if (ii < M && t >= ii && (t - ii) < K) aedge[8*ii +: 8] = a_rd[ii];", "if (t >= ii && (t - ii) < K) aedge[8*ii +: 8] = a_rd[ii];"),
]
F = ["rtl/sram.v", "rtl/dma.v", "rtl/systolic.v", "rtl/requant.v", "rtl/exp_lut.v", "rtl/divu.v", "rtl/rowmem.v", "rtl/ga2_vec.v", "rtl/ga2_sm.v", "rtl/ga2_mm.v", "rtl/ga2.v"]
TB = ["tb/extmem_rw.v", "tb/ga2_tb.v"]
if __name__ == "__main__":
    subprocess.run([sys.executable, "model/ga2_progs.py", "out/ga2_programs.hex", "60", "1"], cwd=hw.ROOT, capture_output=True)
    print("NOTE: this script deliberately breaks copies of the RTL. 'caught' lines are EXPECTED: they show the testbench can detect the mistake.")
    c, n, l = hw.mutate(MUT, F, "ga2_tb", TB, workers=6); print("\n".join(l)); print(f"\nGA-2: {c} of {n} broken circuits caught")
    c2, n2, l2 = hw.mutate(EQUIV, F, "ga2_tb", TB, workers=3); print("\nChanges that look like bugs and are not:"); print("\n".join(l2))
    sys.exit(0 if c == n and c2 == 0 else 1)
