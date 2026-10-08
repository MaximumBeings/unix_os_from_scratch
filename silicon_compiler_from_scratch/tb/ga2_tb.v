// Chapter 7: run many programs on GA-2 and compare each with the Python reference: the whole external memory afterwards, and the five cycle counters.
// For every program: clear the scratchpad, load the program ROM and the external memory image, reset, start, wait for `halted` (or time out).
`timescale 1ns/1ps
`ifndef MAXPRINT
 `define MAXPRINT 5
`endif
`ifndef LAT
 `define LAT 8
`endif
module ga2_tb;
    localparam EXT = 2048, LAT = `LAT;
    reg clk = 0, rst = 1, start = 0; wire halted; wire [15:0] imem_addr; wire [127:0] imem_data;
    wire req_valid; wire [15:0] req_addr; wire resp_valid; wire [31:0] resp_data; wire wr_valid; wire [15:0] wr_addr; wire [31:0] wr_data;
    wire [31:0] cyc_total, cyc_mm, cyc_dma, cyc_vec, n_inst;
    reg [127:0] imem [0:255];
    assign imem_data = imem[imem_addr[7:0]];
    ga2 dut (.clk(clk), .rst(rst), .start(start), .halted(halted), .imem_addr(imem_addr), .imem_data(imem_data),
        .req_valid(req_valid), .req_addr(req_addr), .resp_valid(resp_valid), .resp_data(resp_data), .wr_valid(wr_valid), .wr_addr(wr_addr), .wr_data(wr_data),
        .cyc_total(cyc_total), .cyc_mm(cyc_mm), .cyc_dma(cyc_dma), .cyc_vec(cyc_vec), .n_inst(n_inst));
    extmem_rw #(.LAT(LAT), .DEPTH(EXT)) ext (.clk(clk), .req_valid(req_valid), .req_addr(req_addr), .resp_valid(resp_valid), .resp_data(resp_data), .wr_valid(wr_valid), .wr_addr(wr_addr), .wr_data(wr_data));
    always #5 clk = ~clk;
    reg [31:0] vec [0:4194303]; integer np, p, off, ni, i, w, bad, badprog, waited, total_cycles; reg [31:0] want [0:4];
    initial begin
        $readmemh("out/ga2_programs.hex", vec); np = vec[0]; off = 1; bad = 0; badprog = 0; total_cycles = 0;
        for (p = 0; p < np; p = p + 1) begin
            ni = vec[off]; off = off + 1;
            for (i = 0; i < 256; i = i + 1) imem[i] = 128'd0;
            for (i = 0; i < ni; i = i + 1) begin imem[i] = {vec[off], vec[off+1], vec[off+2], vec[off+3]}; off = off + 4; end
            for (i = 0; i < EXT; i = i + 1) ext.mem[i] = vec[off + i];
            for (i = 0; i < 4096; i = i + 1) dut.sp.mem[i] = 32'd0;
            off = off + EXT;
            @(negedge clk); rst = 1; @(negedge clk); rst = 0; @(negedge clk); start = 1; @(negedge clk); start = 0;
            waited = 0; while (!halted && waited < 100000) begin @(negedge clk); waited = waited + 1; end
            if (!halted) begin $display("FAIL: program %0d did not halt", p); bad = bad + 1; badprog = badprog + 1; end
            else begin
                w = 0;
                for (i = 0; i < EXT; i = i + 1) if (ext.mem[i] !== vec[off + i]) begin w = w + 1; if (bad < `MAXPRINT) $display("MISMATCH program %0d ext[%0d]: got %0h want %0h", p, i, ext.mem[i], vec[off + i]); end
                want[0] = vec[off + EXT]; want[1] = vec[off + EXT + 1]; want[2] = vec[off + EXT + 2]; want[3] = vec[off + EXT + 3]; want[4] = vec[off + EXT + 4];
                if (cyc_total !== want[0] || cyc_mm !== want[1] || cyc_dma !== want[2] || cyc_vec !== want[3] || n_inst !== want[4]) begin
                    w = w + 1; if (bad < `MAXPRINT) $display("MISMATCH program %0d counters: got total=%0d mm=%0d dma=%0d vec=%0d n=%0d want total=%0d mm=%0d dma=%0d vec=%0d n=%0d", p, cyc_total, cyc_mm, cyc_dma, cyc_vec, n_inst, want[0], want[1], want[2], want[3], want[4]);
                end
                if (w != 0) begin bad = bad + w; badprog = badprog + 1; end
                total_cycles = total_cycles + cyc_total;
`ifdef DUMP
                $display("COUNTERS %0d %0d %0d %0d %0d %0d", p, cyc_total, cyc_mm, cyc_dma, cyc_vec, n_inst);
`endif
            end
            off = off + EXT + 5;
        end
        if (bad == 0) $display("PASS: %0d programs match the Python reference (memory and all five cycle counters), %0d cycles in total (exit status 0)", np, total_cycles);
        else begin $display("FAIL: %0d of %0d programs differ", badprog, np); $fatal(1, "ga2 failed"); end
        $finish;
    end
endmodule
