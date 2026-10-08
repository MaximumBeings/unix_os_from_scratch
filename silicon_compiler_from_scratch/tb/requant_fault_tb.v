// Chapter 10: stuck-at fault grading of the synthesized requantizer. The netlist has an injection point on each sampled gate output (port fsel selects which, fval is the stuck value).
// For every fault the testbench applies the golden vectors in order until the netlist's output differs from the golden output, and prints the index of the first vector that detected it (or -1 if none did).
// Usage: -DNFAULT=<number of injection points>; vectors from out/requant_vectors.hex (row count in word 0, as in Chapter 3).
`timescale 1ns/1ps
module requant_fault_tb;
    localparam NF = `NFAULT;
    reg signed [31:0] acc; reg [23:0] m; reg [5:0] s; reg relu; wire [7:0] q; reg [15:0] fsel; reg fval;
    requant_f dut (.acc(acc), .m(m), .s(s), .relu(relu), .q(q), .fsel(fsel), .fval(fval));
    reg [71:0] vec [0:1048575]; reg [71:0] v; integer nrows, i, f, p, first, bad;
    task apply; input integer r; begin v = vec[r]; acc = v[70:39]; m = v[38:15]; s = v[14:9]; relu = v[8]; #1; end endtask
    initial begin
        $readmemh("out/requant_vectors.hex", vec); nrows = vec[0]; fsel = 16'hFFFF; fval = 0; bad = 0;
        for (i = 1; i <= nrows; i = i + 1) begin apply(i); if (q !== v[7:0]) bad = bad + 1; end
        if (bad != 0) begin $display("FAIL: the fault-free netlist differs from the golden model on %0d of %0d vectors", bad, nrows); $fatal(1, "bad netlist"); end
        $display("FAULTFREE ok %0d vectors", nrows);
        for (f = 0; f < 2 * NF; f = f + 1) begin
            fsel = f / 2; fval = f % 2; first = -1;
            for (i = 1; i <= nrows && first < 0; i = i + 1) begin apply(i); if (q !== v[7:0]) first = i; end
            $display("FAULT %0d %0d %0d", f / 2, f % 2, first);
        end
        $finish;
    end
endmodule
