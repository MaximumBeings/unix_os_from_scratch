// Chapter 2: a multiply-accumulate unit, the building block of every matrix engine. Each clock with en = 1 it adds the product a*b to a 32-bit accumulator.
//   clr = 1 starts a new sum: the accumulator becomes the product (if en) or zero (if not). rst clears everything.
//   SAT = 0 : the sum wraps around on overflow (what plain 32-bit hardware does).   SAT = 1 : it saturates at the largest or smallest 32-bit value.
// An 8x8 product needs 16 bits and a sum of k products needs 16 + log2(k) bits: 32 bits are enough for 2^16 products of the worst case, so overflow is rare, but it is real, and what to do about it is a design decision.
module mac #(parameter SAT = 0) (input clk, input rst, input clr, input en, input signed [7:0] a, input signed [7:0] b, output reg signed [31:0] acc);
    wire signed [15:0] p = a * b;
    wire signed [32:0] wide = acc + p;                                  // 33 bits: the sum cannot overflow this
    wire signed [31:0] summed = (SAT == 0) ? wide[31:0] :
                                (wide > 33'sd2147483647) ? 32'sd2147483647 :
                                (wide < -33'sd2147483648) ? -32'sd2147483648 : wide[31:0];
    always @(posedge clk) begin
        if (rst) acc <= 32'sd0;
        else if (clr) acc <= en ? {{16{p[15]}}, p} : 32'sd0;
        else if (en) acc <= summed;
    end
endmodule

// the saturating version as a module of its own, so that synthesis tools (and chapters) can name it
module mac_sat(input clk, input rst, input clr, input en, input signed [7:0] a, input signed [7:0] b, output signed [31:0] acc);
    mac #(.SAT(1)) u(.clk(clk), .rst(rst), .clr(clr), .en(en), .a(a), .b(b), .acc(acc));
endmodule
