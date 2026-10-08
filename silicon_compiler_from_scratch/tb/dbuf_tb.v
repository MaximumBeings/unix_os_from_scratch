// Chapter 5: run the same stream of tiles twice, serially and double-buffered; compare every tile sum with the golden model AND the cycle counts with the schedule model.
// Shape from -DTILE= -DCPW= -DLAT= ; vectors from out/dbuf_vectors.hex: [ntiles, TILE, serial cycles, double-buffered cycles], then the data words, then the golden sum of every tile.
`timescale 1ns/1ps
`ifndef TILE
 `define TILE 16
`endif
`ifndef CPW
 `define CPW 1
`endif
`ifndef LAT
 `define LAT 4
`endif
module dbuf_tb;
    localparam TILE = `TILE, CPW = `CPW, LAT = `LAT, BASE = 5;
    reg clk = 0, rst = 1, start = 0, dbl = 0; reg [7:0] ntiles = 0; wire req_valid; wire [15:0] req_addr; wire resp_valid; wire [31:0] resp_data;
    wire done, sum_valid; wire [31:0] sum_data, cycles;
    extmem #(.LAT(LAT)) ext (.clk(clk), .req_valid(req_valid), .req_addr(req_addr), .resp_valid(resp_valid), .resp_data(resp_data));
    dbuf #(.TILE(TILE), .CPW(CPW)) dut (.clk(clk), .rst(rst), .start(start), .dbl(dbl), .ntiles(ntiles), .base(BASE[15:0]), .req_valid(req_valid), .req_addr(req_addr), .resp_valid(resp_valid), .resp_data(resp_data),
        .done(done), .sum_valid(sum_valid), .sum_data(sum_data), .cycles(cycles));
    always #5 clk = ~clk;
    integer nreq = 0; always @(posedge clk) if (req_valid) nreq = nreq + 1;      // every word requested from external memory
    reg [31:0] vec [0:65535]; integer nt, vt, i, bad, got_n, mode, exp_cyc, meas [0:1];
    initial begin
        $readmemh("out/dbuf_vectors.hex", vec); nt = vec[0]; vt = vec[1]; bad = 0;
        if (vt != TILE) begin $display("FAIL: vectors are for TILE=%0d, testbench is TILE=%0d", vt, TILE); $fatal(1, "shape mismatch"); end
        for (i = 0; i < nt * TILE; i = i + 1) ext.mem[BASE + i] = vec[4 + i];
        for (mode = 0; mode < 2; mode = mode + 1) begin
            @(negedge clk); rst = 1; @(negedge clk); rst = 0; dbl = mode; ntiles = nt; got_n = 0; nreq = 0;
            @(negedge clk); start = 1; @(negedge clk); start = 0;
            while (!done) begin
                @(posedge clk); #1;
                if (sum_valid) begin
                    if (sum_data !== vec[4 + nt * TILE + got_n]) begin bad = bad + 1; if (bad <= 5) $display("MISMATCH mode %0d tile %0d: got %0d want %0d", mode, got_n, sum_data, vec[4 + nt * TILE + got_n]); end
                    got_n = got_n + 1;
                end
            end
            if (nreq != nt * TILE) begin bad = bad + 1; $display("MISMATCH mode %0d: %0d words requested from external memory, expected %0d", mode, nreq, nt * TILE); end
            meas[mode] = cycles; exp_cyc = vec[2 + mode];
            if (got_n != nt) begin bad = bad + 1; $display("MISMATCH mode %0d: %0d sums reported, expected %0d", mode, got_n, nt); end
            if (meas[mode] != exp_cyc) begin bad = bad + 1; $display("MISMATCH mode %0d: %0d cycles, schedule model says %0d", mode, meas[mode], exp_cyc); end
        end
        if (bad == 0) $display("PASS: TILE=%0d CPW=%0d LAT=%0d, %0d tiles: sums match, cycles serial=%0d double-buffered=%0d match the schedule model (exit status 0)", TILE, CPW, LAT, nt, meas[0], meas[1]);
        else begin $display("FAIL: %0d problems", bad); $fatal(1, "dbuf failed"); end
        $finish;
    end
    initial begin #50000000; $display("FAIL: timeout"); $fatal(1, "timeout"); end
endmodule
