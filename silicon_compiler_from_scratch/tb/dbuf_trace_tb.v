// Chapter 5, running example A: run one stream (mode from -DMODE=0 serial or 1 double-buffered; shape from -DTILE -DCPW -DLAT) and print the moments the controller starts and finishes every load and every compute ("E cycle what tile"), sampled in the middle of each cycle. A script draws the timeline from these events.
`timescale 1ns/1ps
`ifndef TILE
 `define TILE 16
`endif
`ifndef CPW
 `define CPW 2
`endif
`ifndef LAT
 `define LAT 4
`endif
`ifndef MODE
 `define MODE 1
`endif
module dbuf_trace_tb;
    localparam TILE = `TILE, CPW = `CPW, LAT = `LAT, BASE = 5;
    reg clk = 0, rst = 1, start = 0, dbl = `MODE; reg [7:0] ntiles = 0; wire req_valid; wire [15:0] req_addr; wire resp_valid; wire [31:0] resp_data; wire done, sum_valid; wire [31:0] sum_data, cycles;
    extmem #(.LAT(LAT)) ext (.clk(clk), .req_valid(req_valid), .req_addr(req_addr), .resp_valid(resp_valid), .resp_data(resp_data));
    dbuf #(.TILE(TILE), .CPW(CPW)) dut (.clk(clk), .rst(rst), .start(start), .dbl(dbl), .ntiles(ntiles), .base(BASE[15:0]), .req_valid(req_valid), .req_addr(req_addr), .resp_valid(resp_valid), .resp_data(resp_data), .done(done), .sum_valid(sum_valid), .sum_data(sum_data), .cycles(cycles));
    always #5 clk = ~clk;
    reg [31:0] vec [0:65535]; integer nt, i;
    always @(negedge clk) if (dut.running) begin
        if (dut.can_load) $display("E %0d LOAD_START %0d", dut.cycles, dut.ld_n);
        if (dut.d_done)   $display("E %0d LOAD_DONE %0d", dut.cycles, dut.ld_n);
        if (dut.can_comp) $display("E %0d COMP_START %0d", dut.cycles, dut.cp_n);
        if (dut.c_finish) $display("E %0d COMP_DONE %0d", dut.cycles, dut.cp_n);
    end
    initial begin
        $readmemh("out/dbuf_vectors.hex", vec); nt = vec[0];
        for (i = 0; i < nt * TILE; i = i + 1) ext.mem[BASE + i] = vec[4 + i];
        @(negedge clk); rst = 1; @(negedge clk); rst = 0; ntiles = nt; @(negedge clk); start = 1; @(negedge clk); start = 0;
        while (!done) @(posedge clk); #1; $display("TOTAL %0d", cycles); $finish;
    end
endmodule
