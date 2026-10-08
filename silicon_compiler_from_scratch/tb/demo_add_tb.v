// Chapter 10, running example A: the three 8-bit adders (tool-chosen '+', ripple chain, Kogge-Stone) against a + b on all 65,536 input pairs. Self-checking: exit status 1 on any mismatch.
`timescale 1ns/1ps
module demo_add_tb;
    reg [7:0] a, b; wire [8:0] s1, s2, s3; integer i, bad = 0;
    demo_add #(8) d1 (a, b, s1); demo_ripple #(8) d2 (a, b, s2); demo_kogge #(8) d3 (a, b, s3);
    initial begin
        for (i = 0; i < 65536; i = i + 1) begin
            a = i[7:0]; b = i[15:8]; #1;
            if (s1 !== a + b || s2 !== a + b || s3 !== a + b) begin bad = bad + 1; if (bad <= 5) $display("MISMATCH a=%0d b=%0d: %0d %0d %0d want %0d", a, b, s1, s2, s3, a + b); end
        end
        if (bad == 0) $display("PASS: all three 8-bit adders match a + b on all 65536 input pairs (exit status 0)"); else begin $display("FAIL: %0d mismatches", bad); $fatal(1, "adders failed"); end
        $finish;
    end
endmodule
