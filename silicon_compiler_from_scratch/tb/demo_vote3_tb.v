// Background page demo testbench: try all eight inputs on the three descriptions, then a clocked example. Self-checking: exit status 1 on a mismatch.
`timescale 1ns/1ps
module demo_vote3_tb;
    reg a, b, c; wire y1, y2, y3; integer i, bad = 0; reg expect_y;
    vote3_assign d1 (a, b, c, y1); vote3_always d2 (a, b, c, y2); vote3_gates d3 (a, b, c, y3);
    reg clk = 0, rst = 1, d = 0; wire ys; vote_seq s (clk, rst, d, ys); always #5 clk = ~clk;
    initial begin
        $display("a b c | assign always gates | expected");
        for (i = 0; i < 8; i = i + 1) begin
            {a, b, c} = i[2:0]; #1; expect_y = (a + b + c) >= 2;
            $display("%b %b %b |   %b      %b      %b   |    %b", a, b, c, y1, y2, y3, expect_y);
            if (y1 !== expect_y || y2 !== expect_y || y3 !== expect_y) bad = bad + 1;
        end
        $display("\nclocked: input d is shifted in on each rising edge; y is the vote of the last three values");
        @(negedge clk); rst = 0;
        for (i = 0; i < 8; i = i + 1) begin
            d = (i == 1 || i == 2 || i == 3 || i == 6) ? 1'b1 : 1'b0; @(posedge clk); #1; $display("cycle %0d  d=%b  window=%b  y=%b", i, d, s.w, ys);
        end
        if (bad == 0) $display("PASS: all three descriptions agree with the majority function (exit status 0)"); else begin $display("FAIL"); $fatal(1, "demo failed"); end
        $finish;
    end
endmodule
