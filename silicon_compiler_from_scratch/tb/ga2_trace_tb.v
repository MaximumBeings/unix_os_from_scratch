// Chapter 7, running examples: run ONE program (out/ga2_trace.hex in ga2_tb's format, count word = 1) and print what the sequencer does: "F pc cycle" when it fetches, "I pc cycle" when it issues, "D pc cycle" when the unit reports done, "H cycle" when it halts; then the counters.
`timescale 1ns/1ps
`ifndef EXTW
 `define EXTW 2048
`endif
module ga2_trace_tb;
    localparam EXT = `EXTW, LAT = 8;
    reg clk = 0, rst = 1, start = 0; wire halted; wire [15:0] imem_addr; wire [127:0] imem_data;
    wire req_valid; wire [15:0] req_addr; wire resp_valid; wire [31:0] resp_data; wire wr_valid; wire [15:0] wr_addr; wire [31:0] wr_data; wire [31:0] cyc_total, cyc_mm, cyc_dma, cyc_vec, n_inst;
    reg [127:0] imem [0:255]; assign imem_data = imem[imem_addr[7:0]];
    ga2 dut (.clk(clk), .rst(rst), .start(start), .halted(halted), .imem_addr(imem_addr), .imem_data(imem_data), .req_valid(req_valid), .req_addr(req_addr), .resp_valid(resp_valid), .resp_data(resp_data), .wr_valid(wr_valid), .wr_addr(wr_addr), .wr_data(wr_data),
        .cyc_total(cyc_total), .cyc_mm(cyc_mm), .cyc_dma(cyc_dma), .cyc_vec(cyc_vec), .n_inst(n_inst));
    extmem_rw #(.LAT(LAT), .DEPTH(EXT)) ext (.clk(clk), .rst(rst), .req_valid(req_valid), .req_addr(req_addr), .resp_valid(resp_valid), .resp_data(resp_data), .wr_valid(wr_valid), .wr_addr(wr_addr), .wr_data(wr_data));
    always #5 clk = ~clk;
    reg [31:0] vec [0:100000]; integer np, off, ni, i, waited;
    always @(negedge clk) if (!rst) begin
        if (dut.st == 1) $display("F %0d %0d", dut.pc, dut.cyc_total);
        if (dut.st == 2) $display("I %0d %0d", dut.pc, dut.cyc_total);
        if (dut.st == 3 && dut.unit_done) $display("D %0d %0d", dut.pc, dut.cyc_total);
    end
    initial begin
        $readmemh("out/ga2_trace.hex", vec); np = vec[0]; off = 1; ni = vec[off]; off = off + 1;
        for (i = 0; i < 256; i = i + 1) imem[i] = 128'd0;
        for (i = 0; i < ni; i = i + 1) begin imem[i] = {vec[off], vec[off+1], vec[off+2], vec[off+3]}; off = off + 4; end
        for (i = 0; i < EXT; i = i + 1) ext.mem[i] = vec[off + i];
        for (i = 0; i < 4096; i = i + 1) dut.sp.mem[i] = 32'd0;
        @(negedge clk); rst = 1; @(negedge clk); rst = 0; @(negedge clk); start = 1; @(negedge clk); start = 0;
        waited = 0; while (!halted && waited < 100000) begin @(negedge clk); waited = waited + 1; end
        $display("H %0d", cyc_total); $display("COUNTERS %0d %0d %0d %0d %0d", cyc_total, cyc_mm, cyc_dma, cyc_vec, n_inst);
        for (i = 0; i < EXT; i = i + 1) if (ext.mem[i] !== 32'd0 && i >= 16) $display("X %0d %0d", i, $signed(ext.mem[i]));
        $finish;
    end
endmodule
