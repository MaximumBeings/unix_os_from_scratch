// Chapter 3: the requantizer against the golden vectors (directed corner cases and 200,000 random ones).
`timescale 1ns/1ps
module requant_tb;
    reg signed [31:0] acc; reg [23:0] m; reg [5:0] s; reg relu; wire signed [7:0] q;
    requant dut(.acc(acc), .m(m), .s(s), .relu(relu), .q(q));
    reg [71:0] vec [0:1048575]; integer i, bad, n, nrows; reg [71:0] v;
    initial begin
        $readmemh("out/requant_vectors.hex", vec); bad = 0;
        nrows = vec[0];
        for (i = 1; i <= nrows; i = i + 1) begin
            v = vec[i]; acc = v[70:39]; m = v[38:15]; s = v[14:9]; relu = v[8]; #1; n = i;
            if (q !== v[7:0]) begin bad = bad + 1; if (bad <= 5) $display("MISMATCH acc=%0d m=%0d s=%0d relu=%0d: got %0d want %0d", acc, m, s, relu, q, $signed(v[7:0])); end
        end
        if (bad == 0) $display("PASS: the requantizer matches the golden model on all %0d cases", n); else begin $display("FAIL: %0d of %0d cases differ", bad, n); $fatal(1, "requant failed"); end
        $finish;
    end
endmodule
