// Chapter 1, running example B: drive the counter with the golden model's inputs and compare wrap (before each clock edge) and q (after it). Self-checking: exit status 1 on any mismatch.
`timescale 1ns/1ps
module counter4_tb;
    reg clk = 0, rst = 0, en = 0; wire [3:0] q; wire wrap; reg [15:0] vec [0:8191]; integer n, i, bad; reg [15:0] v;
    counter4 dut (.clk(clk), .rst(rst), .en(en), .q(q), .wrap(wrap));
    always #5 clk = ~clk;
    initial begin
        $readmemh("out/counter4_vectors.hex", vec); n = vec[0]; bad = 0;
        for (i = 1; i <= n; i = i + 1) begin
            v = vec[i];
            @(negedge clk); rst = v[6]; en = v[5]; #1;                       // inputs change in the middle of the cycle, away from the clock edge
            if (wrap !== v[4]) begin bad = bad + 1; if (bad <= 5) $display("MISMATCH cycle %0d: wrap is %b, expected %b (q=%0d en=%b)", i, wrap, v[4], q, en); end
            @(posedge clk); #1;                                              // after the edge, q has its new value
            if (q !== v[3:0]) begin bad = bad + 1; if (bad <= 5) $display("MISMATCH cycle %0d: q is %0d, expected %0d (rst=%b en=%b)", i, q, v[3:0], rst, en); end
        end
        if (bad == 0) $display("PASS: the counter matches the golden model for all %0d clock cycles (exit status 0)", n);
        else begin $display("FAIL: %0d mismatches", bad); $fatal(1, "counter4 failed"); end
        $finish;
    end
endmodule
