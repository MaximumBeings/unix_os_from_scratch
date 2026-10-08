// Chapter 10, running example B: the Chapter 3 requantizer cut into two pipeline stages by one register. Stage 1 takes the magnitude of the accumulator and multiplies it by the mantissa (the big 32 x 24 multiplier); the register holds the 56-bit product and the few bits stage 2 still needs; stage 2 adds the rounding constant, shifts, clamps and restores the sign.
// Same function as `requant`, delayed by ONE clock cycle: the q that appears after clock edge n is the requantization of the inputs that were present just before it.
module rq_stage1 (input signed [31:0] acc, input [23:0] m, output neg, output [55:0] prod);
    assign neg = acc[31];
    wire [31:0] mag = neg ? (~acc + 32'd1) : acc;
    assign prod = mag * m;
endmodule
module rq_stage2 (input neg, input [55:0] prod, input [5:0] s, input relu, output signed [7:0] q);
    wire [55:0] half = (s == 6'd0) ? 56'd0 : (56'd1 << (s - 6'd1));
    wire [56:0] sum = {1'b0, prod} + {1'b0, half};
    wire [56:0] shifted = sum >> s;
    wire [6:0] mag_q = (shifted > 57'd127) ? 7'd127 : shifted[6:0];
    wire signed [7:0] signed_q = neg ? -$signed({1'b0, mag_q}) : $signed({1'b0, mag_q});
    assign q = (relu && signed_q[7]) ? 8'sd0 : signed_q;
endmodule
module requant_p2 (input clk, input signed [31:0] acc, input [23:0] m, input [5:0] s, input relu, output signed [7:0] q);
    wire neg1; wire [55:0] prod1; rq_stage1 u1 (.acc(acc), .m(m), .neg(neg1), .prod(prod1));
    reg neg_r; reg [55:0] prod_r; reg [5:0] s_r; reg relu_r;                       // the pipeline register: 64 flip-flops
    always @(posedge clk) begin neg_r <= neg1; prod_r <= prod1; s_r <= s; relu_r <= relu; end
    rq_stage2 u2 (.neg(neg_r), .prod(prod_r), .s(s_r), .relu(relu_r), .q(q));
endmodule

// Variant: the same two stages, but stage 2's 57-bit rounding addition is a Kogge-Stone prefix adder (rtl/demo_add.v) instead of the ripple chain the synthesis tool builds for "+".
module rq_stage2k (input neg, input [55:0] prod, input [5:0] s, input relu, output signed [7:0] q);
    wire [55:0] half = (s == 6'd0) ? 56'd0 : (56'd1 << (s - 6'd1));
    wire [56:0] sum; demo_kogge #(.W(56)) add (.a(prod), .b(half), .s(sum));
    wire [56:0] shifted = sum >> s;
    wire [6:0] mag_q = (shifted > 57'd127) ? 7'd127 : shifted[6:0];
    wire signed [7:0] signed_q = neg ? -$signed({1'b0, mag_q}) : $signed({1'b0, mag_q});
    assign q = (relu && signed_q[7]) ? 8'sd0 : signed_q;
endmodule
module requant_p2k (input clk, input signed [31:0] acc, input [23:0] m, input [5:0] s, input relu, output signed [7:0] q);
    wire neg1; wire [55:0] prod1; rq_stage1 u1 (.acc(acc), .m(m), .neg(neg1), .prod(prod1));
    reg neg_r; reg [55:0] prod_r; reg [5:0] s_r; reg relu_r;
    always @(posedge clk) begin neg_r <= neg1; prod_r <= prod1; s_r <= s; relu_r <= relu; end
    rq_stage2k u2 (.neg(neg_r), .prod(prod_r), .s(s_r), .relu(relu_r), .q(q));
endmodule
