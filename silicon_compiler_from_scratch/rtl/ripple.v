`timescale 1ns/1ns
// Chapter 1, running example A: the 4-bit ripple-carry adder again, but with a delay of one time unit in every full adder, so that a simulator shows the carry rippling from stage to stage.
// The delays exist only in simulation (`assign #1`); synthesis ignores them. Real gates are not instantaneous, and this is the simplest way to see what that does.
module fa_delay (input a, input b, input cin, output sum, output cout);
    assign #1 sum  = a ^ b ^ cin;
    assign #1 cout = (a & b) | (cin & (a ^ b));
endmodule
module ripple4 (input [3:0] a, input [3:0] b, input cin, output [3:0] sum, output cout);
    wire [4:0] c; assign c[0] = cin;
    genvar i;
    generate for (i = 0; i < 4; i = i + 1) begin : stage
        fa_delay fa (.a(a[i]), .b(b[i]), .cin(c[i]), .sum(sum[i]), .cout(c[i+1]));
    end endgenerate
    assign cout = c[4];
endmodule
