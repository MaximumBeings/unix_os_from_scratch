// Appendix B testbench: self-checking. Gray counter against the formula and the one-bit-change property; the swap modules; the extension module.
`timescale 1ns/1ps
module appx_b_tb;
    reg clk = 0, rst = 1, en = 0; wire [3:0] g; wire [3:0] a1, b1, a2, b2; wire [11:0] sext, zext; reg [7:0] x = 0; integer i, errs = 0, ones; reg [3:0] prev;
    gray_cnt #(4) dut (.clk(clk), .rst(rst), .en(en), .g(g)); swap_nb s1 (.clk(clk), .rst(rst), .a(a1), .b(b1)); swap_blocking s2 (.clk(clk), .rst(rst), .a(a2), .b(b2)); extend e (.x(x), .sext(sext), .zext(zext));
    always #5 clk = ~clk;
    initial begin
        @(negedge clk); @(negedge clk); rst = 0; en = 1; prev = g;
        for (i = 1; i <= 20; i = i + 1) begin
            @(negedge clk); ones = (g ^ prev); ones = (ones == 1) | (ones == 2) | (ones == 4) | (ones == 8);
            if (g !== ((i[3:0]) ^ (i[3:0] >> 1))) begin errs = errs + 1; $display("MISMATCH step %0d: gray %b, expected %b", i, g, (i[3:0]) ^ (i[3:0] >> 1)); end
            if (!ones) begin errs = errs + 1; $display("MISMATCH step %0d: more than one bit changed (%b -> %b)", i, prev, g); end
            prev = g;
        end
        en = 0; prev = g; repeat (3) @(negedge clk);
        if (g !== prev) begin errs = errs + 1; $display("MISMATCH the counter moved while enable was 0"); end
        en = 1; @(negedge clk); if (g === prev) begin errs = errs + 1; $display("MISMATCH the counter did not move when enable returned"); end
        $display("gray counter: 20 steps checked (value = binary ^ binary>>1, exactly one bit changes per step); holds its value for 3 edges with enable low");
        $display("after reset and 20 clock edges: non-blocking swap a=%0d b=%0d (the pair swaps at every edge, so after an even number of edges it is back at 1,2); blocking swap a=%0d b=%0d (both ended equal at the first edge)", a1, b1, a2, b2);
        if (!(a1 == 4'd1 && b1 == 4'd2)) begin errs = errs + 1; $display("MISMATCH non-blocking swap"); end
        if (!(a2 == b2)) begin errs = errs + 1; $display("MISMATCH blocking swap should end equal"); end
        x = 8'h80; #1; $display("x = 8'h80: sign-extended %h (%0d), zero-extended %h (%0d)", sext, $signed(sext), zext, zext);
        if (sext !== 12'hF80 || zext !== 12'h080) begin errs = errs + 1; $display("MISMATCH extension"); end
        x = 8'h7F; #1; if (sext !== 12'h07F) begin errs = errs + 1; $display("MISMATCH extension of a positive value"); end
        if (errs == 0) $display("PASS: appendix B demonstration modules behave as described"); else $display("FAIL: %0d problems", errs);
        $finish;
    end
endmodule
