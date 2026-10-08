// Chapter 10, running example A: two adders of any width. demo_add writes "+" and lets the synthesis tool choose the structure; demo_ripple builds a chain of full adders by hand (the Chapter 1 ripple-carry design).
module demo_add #(parameter W = 8) (input [W-1:0] a, input [W-1:0] b, output [W:0] s);
    assign s = a + b;
endmodule
module demo_ripple #(parameter W = 8) (input [W-1:0] a, input [W-1:0] b, output [W:0] s);
    wire [W:0] c; assign c[0] = 1'b0;
    genvar i;
    generate for (i = 0; i < W; i = i + 1) begin : fa
        assign s[i] = a[i] ^ b[i] ^ c[i];
        assign c[i+1] = (a[i] & b[i]) | (c[i] & (a[i] ^ b[i]));
    end endgenerate
    assign s[W] = c[W];
endmodule
// A parallel-prefix (Kogge-Stone) adder for W a power of two: the carry into every bit is computed in log2(W) levels instead of rippling through W stages.
//   g = a & b (this bit generates a carry), p = a ^ b (this bit propagates one). Level k combines each bit's (g, p) with that of the bit 2^k places below: G' = G | (P & G_below), P' = P & P_below.
module demo_kogge #(parameter W = 8) (input [W-1:0] a, input [W-1:0] b, output [W:0] s);
    localparam LV = $clog2(W);
    wire [W-1:0] G [0:LV]; wire [W-1:0] P [0:LV];
    assign G[0] = a & b; assign P[0] = a ^ b;
    genvar l, i;
    generate for (l = 0; l < LV; l = l + 1) begin : lvl
        for (i = 0; i < W; i = i + 1) begin : bit_
            if (i >= (1 << l)) begin : comb
                assign G[l+1][i] = G[l][i] | (P[l][i] & G[l][i - (1 << l)]);
                assign P[l+1][i] = P[l][i] & P[l][i - (1 << l)];
            end else begin : pass
                assign G[l+1][i] = G[l][i]; assign P[l+1][i] = P[l][i];
            end
        end
    end endgenerate
    assign s[0] = P[0][0];
    generate for (i = 1; i < W; i = i + 1) begin : sm assign s[i] = P[0][i] ^ G[LV][i-1]; end endgenerate
    assign s[W] = G[LV][W-1];
endmodule
