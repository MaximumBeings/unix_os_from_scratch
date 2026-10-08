// Chapter 3: the requantizer. It turns the int32 accumulator of a matrix product back into an int8 for the next layer:
//    q = clamp( round_half_away( acc * m / 2^s ), -127, 127 )          (clamped at 0 from below when relu = 1)
// m is a 24-bit mantissa, s a shift of 0 to 63. Rounding is to nearest with ties away from zero, done on the magnitude so that the result is symmetric: requant(-x) = -requant(x).
// Combinational: one 32 x 24 multiplier, one adder for the rounding constant, a shifter and a clamp.
module requant(input signed [31:0] acc, input [23:0] m, input [5:0] s, input relu, output signed [7:0] q);
    wire neg = acc[31];
    wire [31:0] mag = neg ? (~acc + 32'd1) : acc;                     // |acc| (2^31 fits in 32 unsigned bits)
    wire [55:0] prod = mag * m;                                       // up to 2^31 * 2^24 = 2^55
    wire [55:0] half = (s == 6'd0) ? 56'd0 : (56'd1 << (s - 6'd1));   // 2^(s-1): half a unit in the last place kept
    wire [56:0] sum = {1'b0, prod} + {1'b0, half};
    wire [56:0] shifted = sum >> s;                                   // |result| before the clamp
    wire [56:0] limit = 57'd127;
    wire [6:0] mag_q = (shifted > limit) ? 7'd127 : shifted[6:0];     // clamp the magnitude at 127
    wire signed [7:0] signed_q = neg ? -$signed({1'b0, mag_q}) : $signed({1'b0, mag_q});
    assign q = (relu && signed_q[7]) ? 8'sd0 : signed_q;
endmodule
