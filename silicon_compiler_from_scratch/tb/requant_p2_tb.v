// Chapter 10, running example B: the two-stage requantizer against the same golden vectors as the combinational one (out/requant_vectors.hex), with its one-cycle latency: apply inputs, wait for one clock edge, compare. Self-checking.
`timescale 1ns/1ps
module requant_p2_tb;
    reg clk = 0; reg signed [31:0] acc = 0; reg [23:0] m = 0; reg [5:0] s = 0; reg relu = 0; wire signed [7:0] q;
    requant_p2 dut (.clk(clk), .acc(acc), .m(m), .s(s), .relu(relu), .q(q)); always #5 clk = ~clk;
    reg [71:0] vec [0:1048575]; integer i, bad, n, nrows; reg [71:0] v;
    initial begin
        $readmemh("out/requant_vectors.hex", vec); bad = 0; nrows = vec[0];
        for (i = 1; i <= nrows; i = i + 1) begin
            v = vec[i]; @(negedge clk); acc = v[70:39]; m = v[38:15]; s = v[14:9]; relu = v[8]; @(posedge clk); #1; n = i;       // inputs set before the edge; the result is visible just after it
            if (q !== v[7:0]) begin bad = bad + 1; if (bad <= 5) $display("MISMATCH acc=%0d m=%0d s=%0d relu=%0d: got %0d want %0d", acc, m, s, relu, q, $signed(v[7:0])); end
        end
        if (bad == 0) $display("PASS: the two-stage requantizer matches the golden model on all %0d cases (exit status 0)", n); else begin $display("FAIL: %0d of %0d cases differ", bad, n); $fatal(1, "requant_p2 failed"); end
        $finish;
    end
endmodule
