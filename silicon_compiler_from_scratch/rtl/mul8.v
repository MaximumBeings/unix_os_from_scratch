// Chapter 2: two signed 8x8 -> 16 bit multipliers with the same behaviour and different structure.
// mul8_beh lets the synthesis tool choose the structure (the Verilog operator *); mul8_sa spells out a shift-and-add array: one row per bit of b, each row a shifted copy of a,
// and the sign bit of b (bit 7, worth -128) SUBTRACTS its row. In two's complement an n-bit number is  -2^(n-1) * bit(n-1) + sum of 2^i * bit(i).
module mul8_beh(input signed [7:0] a, input signed [7:0] b, output signed [15:0] p);
    assign p = a * b;
endmodule

module mul8_sa(input signed [7:0] a, input signed [7:0] b, output signed [15:0] p);
    wire signed [15:0] ax = a;                       // a sign-extended to the product's width
    wire signed [15:0] r0 = b[0] ? (ax <<< 0) : 16'sd0;
    wire signed [15:0] r1 = b[1] ? (ax <<< 1) : 16'sd0;
    wire signed [15:0] r2 = b[2] ? (ax <<< 2) : 16'sd0;
    wire signed [15:0] r3 = b[3] ? (ax <<< 3) : 16'sd0;
    wire signed [15:0] r4 = b[4] ? (ax <<< 4) : 16'sd0;
    wire signed [15:0] r5 = b[5] ? (ax <<< 5) : 16'sd0;
    wire signed [15:0] r6 = b[6] ? (ax <<< 6) : 16'sd0;
    wire signed [15:0] r7 = b[7] ? (ax <<< 7) : 16'sd0;  // the row for the sign bit
    assign p = r0 + r1 + r2 + r3 + r4 + r5 + r6 - r7;
endmodule
