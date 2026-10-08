// Chapter 2: exhaustive test of both multipliers: all 65,536 input pairs against the answers the Python golden model wrote.
`timescale 1ns/1ps
module mul8_tb;
    reg signed [7:0] a, b; wire signed [15:0] p1, p2;
    mul8_beh d1(.a(a), .b(b), .p(p1)); mul8_sa d2(.a(a), .b(b), .p(p2));
    reg [31:0] vec [0:65535]; integer i, bad1, bad2; reg [31:0] v;
    initial begin
        $readmemh("out/mul8_vectors.hex", vec); bad1 = 0; bad2 = 0;
        for (i = 0; i < 65536; i = i + 1) begin
            v = vec[i]; a = v[31:24]; b = v[23:16]; #1;
            if (p1 !== v[15:0]) begin bad1 = bad1 + 1; if (bad1 <= 3) $display("mul8_beh MISMATCH %0d * %0d = %0d, want %0d", a, b, p1, $signed(v[15:0])); end
            if (p2 !== v[15:0]) begin bad2 = bad2 + 1; if (bad2 <= 3) $display("mul8_sa MISMATCH %0d * %0d = %0d, want %0d", a, b, p2, $signed(v[15:0])); end
        end
        if (bad1 == 0 && bad2 == 0) $display("PASS: both multipliers match the golden model on all 65536 input pairs");
        else begin $display("FAIL: mul8_beh %0d, mul8_sa %0d of 65536 pairs differ", bad1, bad2); $fatal(1, "mul8 failed"); end
        $finish;
    end
endmodule
